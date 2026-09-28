"""Reuse the C++ runner process while constructing fresh caches for every run."""

import subprocess

from cache_benchmark import check_statistics, write_config_file, write_input_file


class Runner:
    def __init__(self, path, folder):
        self.folder = folder
        folder.mkdir(parents=True, exist_ok=True)
        self.error_file = (folder / "runner.stderr").open("w+", encoding="utf-8")
        try:
            self.process = subprocess.Popen(
                [str(path), "--batch"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=self.error_file,
                text=True,
                encoding="utf-8",
            )
        except OSError:
            self.error_file.close()
            raise

        self.configuration_paths = {}
        self.last_requests = None
        self.last_capacities = None

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception, traceback):
        self.close()

    def run(self, strategies, capacities, requests):
        if strategies not in self.configuration_paths:
            configuration_path = self.folder / ("_".join(strategies) + ".txt")
            write_config_file(configuration_path, strategies)
            self.configuration_paths[strategies] = configuration_path

        input_path = self.folder / "current_input.txt"
        # A response confirms that the preceding experiment has finished reading.
        if self.last_requests is not requests or self.last_capacities != capacities:
            write_input_file(input_path, capacities, requests)
            self.last_requests = requests
            self.last_capacities = capacities

        self.process.stdin.write(
            str(self.configuration_paths[strategies]) + "\n" + str(input_path) + "\n"
        )
        self.process.stdin.flush()
        response = self.process.stdout.readline()
        if not response:
            self.error_file.seek(0)
            raise RuntimeError("Runner stopped: " + self.error_file.read())

        counters = [int(value) for value in response.split()]
        check_statistics(counters, len(requests), len(capacities))
        return counters

    def close(self):
        if self.process.stdin.closed:
            return
        try:
            self.process.stdin.close()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        finally:
            self.process.stdout.close()
            self.error_file.close()
