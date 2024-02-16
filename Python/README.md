# Installing

```bash
cd Python
sudo apt install i2c-tools python3-smbus python3-rpi.gpio python3-gpiod gcc python3-dev
sudo apt install pipx
pipx ensurepath
pipx install poetry
poetry completions bash >> ~/.bash_completion
```

```bash
cd Raspberry_Pi
poetry init
poetry add jinja2 psutil pyserial requests gpiod
poetry shell
```

```bash
python 