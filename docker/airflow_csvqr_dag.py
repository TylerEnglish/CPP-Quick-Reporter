from airflow import DAG
from airflow.providers.docker.operators.docker import DockerOperator
from datetime import datetime

with DAG(
    dag_id="csvqr_report",
    start_date=datetime(2025, 1, 1),
    schedule_interval="@daily",
    catchup=False,
) as dag:
    run_report = DockerOperator(
        task_id="run_csvqr",
        image="csvqr:latest",
        api_version="auto",
        auto_remove=True,
        command="--input /app/data/samples/simple.csv --output-root /app/artifacts --project-id {{ ds_nodash }}",
        mounts=[
            # adjust to your Airflow host paths
            # Mount your host data and artifacts
            {"source": "/opt/airflow/data", "target": "/app/data", "type": "bind"},
            {"source": "/opt/airflow/artifacts", "target": "/app/artifacts", "type": "bind"},
        ],
        docker_url="unix://var/run/docker.sock",
        network_mode="bridge",
    )
