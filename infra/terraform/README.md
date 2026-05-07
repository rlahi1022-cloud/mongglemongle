# 몽글몽글 AWS 인프라 (Terraform)

기획 14장의 구도를 코드로 옮긴 스켈레톤. 실제 `terraform apply`는 별도 검토 후 실행.

## 구성 요소

| 리소스 | 역할 |
|---|---|
| VPC + 2 AZ public/private subnets | 격리, Multi-AZ |
| ALB (HTTPS) → EC2 × 2 | 메인서버 (Drogon, C++) |
| RDS MariaDB Multi-AZ (`db.r6g.large`) | 영속 데이터, 7일 백업 |
| ElastiCache Redis (replication, multi-AZ) | L2 캐시, fanout, rate limit |
| S3 `monggle-media-{env}` | 원본/썸네일, 30일 IA → 90일 Glacier |
| S3 `monggle-archive-{env}` | 일괄 다운로드 zip, 7일 만료 |
| CloudFront + OAI | 미디어 CDN, 메인서버 우회 |
| EC2 × 1 (AI 허브) | Python FastAPI, BGE-m3 임베딩 |

## 사용

```bash
cd infra/terraform
terraform init
terraform plan -var="db_password=$(openssl rand -hex 24)"
# 검토 후
terraform apply -var="db_password=..."
```

## 보류 (별도 파일로 분리 권장)

- EC2 launch_template / userdata (Drogon 빌드 + systemd 서비스)
- Auto Scaling Group + Target Group attachment
- Route 53 record (도메인 구매 후)
- IAM 역할 (S3 R/W, CloudWatch Logs, KMS for JWT 키)
- 부하 테스트 시나리오 (k6/wrk) — 별도 `infra/loadtest/`
