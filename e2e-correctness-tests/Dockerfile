FROM python:3.6
RUN apt update; apt install -y cmake
COPY /e2e-correctness-tests/requirements.txt /app/requirements.txt
RUN pip3 install -r /app/requirements.txt
COPY /e2e-correctness-tests/ /app/
WORKDIR /app
CMD ["python", "-m", "pytest", "--log-cli-level=INFO", ""]
