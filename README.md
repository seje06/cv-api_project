# cv-api_project
개발자가 영상 링크로 서버에 요청하여 처리할 수 있는 api 개발 프로젝트


## 세팅

- 확장에서 Docker 다운

    ![alt text](image.png)

- 확장에서 Dev Containers 다운

    ![alt text](image-2.png)

## 빌드 및 테스트

- bash 에서 아래 명령문 하나씩 치기
    - 빌드
        ```bash
        > cmake -S . -B build
        > cmake --build build -j
        ```
    - 서버 실행
        ```bash
        > ./build/ImageApiServer
        ```

- 새로 bash 열어서 아래 명령문 치기
    - 서버 열렸는지 확인
        ```bash
        curl http://localhost:8080/health
        ```
    - 이미지 링크 넘겨주고 처리된거 받기
        ```bash
        curl -X POST http://localhost:8080/api/v1/process \
        -H "Content-Type: application/json" \
        -d '{
            "image_url": "https://image.utoimage.com/preview/cp872722/2022/12/202212008462_500.jpg",
            "action": "grayscale"
        }' \
        --output result.jpg
        ```
    
- 처리 결과(흑백 요청)

    ![alt text](image-1.png)
