// 몽글몽글 AWS 인프라 — 기획 14장 구도 그대로.
//
// [Route 53]
//      ↓
// [CloudFront]                   ← 정적/미디어 CDN
//      ↓
// [ALB (Application LB)]
//      ↓
// [EC2 × 2 (메인서버 C++)]        ← Multi-AZ, ASG로 묶음
//      ↓                ↓
// [RDS MariaDB]   [ElastiCache Redis]
//      ↓
// [S3]
//   ├─ monggle-media     (원본/썸네일)
//   └─ monggle-archive   (일괄 다운로드 zip)
// [EC2 × 1 (AI 허브 Python)]
//      └─ 외부 LLM API
//
// 이 디렉토리는 *코드*만 — `terraform apply`는 별도 검토 후 실행.

terraform {
  required_version = ">= 1.6"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.70"
    }
  }
  // 운영 시 backend "s3" 로 state 분리. 지금은 로컬.
}

provider "aws" {
  region = var.region
}

// ---------- VPC ----------
resource "aws_vpc" "main" {
  cidr_block           = "10.20.0.0/16"
  enable_dns_hostnames = true
  enable_dns_support   = true
  tags = local.tags
}

resource "aws_subnet" "public" {
  count                   = 2
  vpc_id                  = aws_vpc.main.id
  cidr_block              = cidrsubnet(aws_vpc.main.cidr_block, 8, count.index)
  availability_zone       = data.aws_availability_zones.available.names[count.index]
  map_public_ip_on_launch = true
  tags                    = merge(local.tags, { Name = "monggle-public-${count.index}" })
}

resource "aws_subnet" "private" {
  count             = 2
  vpc_id            = aws_vpc.main.id
  cidr_block        = cidrsubnet(aws_vpc.main.cidr_block, 8, count.index + 10)
  availability_zone = data.aws_availability_zones.available.names[count.index]
  tags              = merge(local.tags, { Name = "monggle-private-${count.index}" })
}

resource "aws_internet_gateway" "main" {
  vpc_id = aws_vpc.main.id
  tags   = local.tags
}

data "aws_availability_zones" "available" { state = "available" }

// ---------- Security groups ----------
resource "aws_security_group" "alb" {
  name        = "monggle-alb"
  description = "ALB ingress (HTTPS)"
  vpc_id      = aws_vpc.main.id
  ingress { from_port = 443 to_port = 443 protocol = "tcp" cidr_blocks = ["0.0.0.0/0"] }
  egress  { from_port = 0   to_port = 0   protocol = "-1"  cidr_blocks = ["0.0.0.0/0"] }
  tags = local.tags
}

resource "aws_security_group" "app" {
  name        = "monggle-app"
  description = "Main server EC2"
  vpc_id      = aws_vpc.main.id
  ingress { from_port = 8080 to_port = 8080 protocol = "tcp" security_groups = [aws_security_group.alb.id] }
  egress  { from_port = 0    to_port = 0    protocol = "-1"  cidr_blocks = ["0.0.0.0/0"] }
  tags = local.tags
}

resource "aws_security_group" "db" {
  name        = "monggle-db"
  description = "RDS MariaDB"
  vpc_id      = aws_vpc.main.id
  ingress { from_port = 3306 to_port = 3306 protocol = "tcp" security_groups = [aws_security_group.app.id] }
  tags = local.tags
}

resource "aws_security_group" "redis" {
  name   = "monggle-redis"
  vpc_id = aws_vpc.main.id
  ingress { from_port = 6379 to_port = 6379 protocol = "tcp" security_groups = [aws_security_group.app.id] }
  tags = local.tags
}

// ---------- RDS MariaDB Multi-AZ ----------
resource "aws_db_subnet_group" "main" {
  name       = "monggle-db-subnets"
  subnet_ids = aws_subnet.private[*].id
  tags       = local.tags
}

resource "aws_db_instance" "main" {
  identifier              = "monggle-mariadb"
  engine                  = "mariadb"
  engine_version          = "11.4"
  instance_class          = "db.r6g.large"   // 기획 14.3: 메모리 우선
  allocated_storage       = 100
  storage_type            = "gp3"
  multi_az                = true
  db_subnet_group_name    = aws_db_subnet_group.main.name
  vpc_security_group_ids  = [aws_security_group.db.id]
  username                = "monggle"
  password                = var.db_password         // tfvars에서 주입
  backup_retention_period = 7
  deletion_protection     = true
  skip_final_snapshot     = false
  tags                    = local.tags
}

// ---------- ElastiCache Redis ----------
resource "aws_elasticache_subnet_group" "main" {
  name       = "monggle-redis-subnets"
  subnet_ids = aws_subnet.private[*].id
}

resource "aws_elasticache_replication_group" "main" {
  replication_group_id       = "monggle-redis"
  description                = "Monggle L2 cache + fanout"
  node_type                  = "cache.t4g.medium"
  num_cache_clusters         = 2
  automatic_failover_enabled = true
  multi_az_enabled           = true
  subnet_group_name          = aws_elasticache_subnet_group.main.name
  security_group_ids         = [aws_security_group.redis.id]
  tags                       = local.tags
}

// ---------- S3 ----------
resource "aws_s3_bucket" "media" {
  bucket = "monggle-media-${var.env}"
  tags   = local.tags
}

resource "aws_s3_bucket_versioning" "media" {
  bucket = aws_s3_bucket.media.id
  versioning_configuration { status = "Enabled" }
}

resource "aws_s3_bucket_server_side_encryption_configuration" "media" {
  bucket = aws_s3_bucket.media.id
  rule { apply_server_side_encryption_by_default { sse_algorithm = "AES256" } }
}

resource "aws_s3_bucket_lifecycle_configuration" "media" {
  bucket = aws_s3_bucket.media.id
  rule {
    id     = "tier-down"
    status = "Enabled"
    transition { days = 30  storage_class = "STANDARD_IA" }
    transition { days = 90  storage_class = "GLACIER"     }
  }
}

resource "aws_s3_bucket" "archive" {
  bucket = "monggle-archive-${var.env}"
  tags   = local.tags
}

resource "aws_s3_bucket_lifecycle_configuration" "archive" {
  bucket = aws_s3_bucket.archive.id
  rule {
    id     = "auto-expire"
    status = "Enabled"
    expiration { days = 7 }
  }
}

// ---------- CloudFront ----------
resource "aws_cloudfront_origin_access_identity" "media" {
  comment = "monggle-media OAI"
}

resource "aws_cloudfront_distribution" "media" {
  enabled             = true
  default_root_object = ""
  origin {
    origin_id   = "monggle-media-s3"
    domain_name = aws_s3_bucket.media.bucket_regional_domain_name
    s3_origin_config {
      origin_access_identity = aws_cloudfront_origin_access_identity.media.cloudfront_access_identity_path
    }
  }
  default_cache_behavior {
    target_origin_id       = "monggle-media-s3"
    allowed_methods        = ["GET", "HEAD"]
    cached_methods         = ["GET", "HEAD"]
    viewer_protocol_policy = "redirect-to-https"
    forwarded_values {
      query_string = true
      cookies      { forward = "none" }
    }
    min_ttl     = 0
    default_ttl = 3600
    max_ttl     = 86400
  }
  restrictions { geo_restriction { restriction_type = "none" } }
  viewer_certificate { cloudfront_default_certificate = true }
  tags = local.tags
}

// ---------- ALB ----------
resource "aws_lb" "main" {
  name               = "monggle-alb"
  internal           = false
  load_balancer_type = "application"
  security_groups    = [aws_security_group.alb.id]
  subnets            = aws_subnet.public[*].id
  tags               = local.tags
}

resource "aws_lb_target_group" "app" {
  name        = "monggle-app"
  port        = 8080
  protocol    = "HTTP"
  vpc_id      = aws_vpc.main.id
  target_type = "instance"
  health_check {
    path                = "/healthz"
    healthy_threshold   = 2
    unhealthy_threshold = 3
    interval            = 10
  }
}

// ---------- EC2 (메인서버 × 2 + AI 허브 × 1) ----------
// 실제 launch_template / autoscaling_group / userdata는 별도 파일로 분리 권장.
// 여기서는 자리만.
data "aws_ami" "ubuntu" {
  most_recent = true
  owners      = ["099720109477"]
  filter { name = "name" values = ["ubuntu/images/hvm-ssd-gp3/ubuntu-noble-24.04-amd64-server-*"] }
}

variable "region"      { type = string  default = "ap-northeast-2" }
variable "env"         { type = string  default = "dev" }
variable "db_password" { type = string  sensitive = true }

locals {
  tags = {
    Project = "monggle"
    Env     = var.env
    Owner   = "monggle-team"
  }
}

output "alb_dns"        { value = aws_lb.main.dns_name }
output "rds_endpoint"   { value = aws_db_instance.main.endpoint        sensitive = true }
output "redis_endpoint" { value = aws_elasticache_replication_group.main.primary_endpoint_address }
output "media_bucket"   { value = aws_s3_bucket.media.bucket }
output "cdn_domain"     { value = aws_cloudfront_distribution.media.domain_name }
