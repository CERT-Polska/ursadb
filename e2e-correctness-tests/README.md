# e2e correctness tests

Tests for Ursadb database.

## Generation of queries

To generate queries based on Yara rules put all rules in the yararules directory.
Then run from main e2e-correctness-tests directory to generate queries from all files:
```
python3 generate_queries.py
```
In order to generate query based on the specific yara rule file run:
```
python3 generate_queries.py example_yararule
```
If files are correctly generated you should see .txt files under each corresponding yara rule.
Check if queries are properly generated to exclude any other errors.


## Running tests

Set password for unzipping malware files:
```
export MALWARE_PASSWORD="your_password"
```

Then run:
```
pytest --log-cli-level=INFO
```