import time
import signal
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager

import requests
import click


REQUEST_TIMEOUT = 3
THREADS_RUNNING = True


@click.command()
@click.option("-d", "--delay", help="Delay in seconds between sucessive requests", type=float, default=0.1)
@click.option("-c", "--concurrency", help="Num threads to run", type=int, default=1)
@click.argument("url", type=str)
def main(delay, concurrency, url):
    with ThreadPoolExecutor(concurrency) as pool:
        futures = [pool.submit(run_requests, url, delay) for _ in range(concurrency)]
        with capture_signals():
            results = [f.result() for f in futures]


@contextmanager
def capture_signals():
    old_int_handler = signal.signal(signal.SIGINT, _stop_threads)
    yield
    signal.signal(signal.SIGINT, old_int_handler)


def _stop_threads(sig, frame):
    THREADS_RUNNING = False


def run_requests(url, delay):
    session = requests.Session()
    while THREADS_RUNNING:
        with session.get(url, timeout=REQUEST_TIMEOUT, stream=True) as r:
            content_len = len(r.content)
            click.echo(f"Received response of size {content_len} bytes")
        time.sleep(delay)


if __name__ == "__main__":
    main()
