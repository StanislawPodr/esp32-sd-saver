Simple esp32 SPIFFS and input parsing demo.

## Serial CLI

The firmware exposes a line based UART protocol at 115200 8N1. Use the Python
CLI to talk to it:

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt

python cli.py get my_var
python cli.py set my_var int 42
python cli.py set my_var bool true
python cli.py set my_var float 3.14
python cli.py del my_var
```

The port defaults to `/dev/ttyUSB0`; change it with `--port` (or `-p`).
By default only replies to `get` are printed — firmware logs on the same UART
are suppressed. Add `--verbose` to see all serial traffic.
