# HABITAT
Remote RID with Linux Debian OS for [Habitat Project](www.habitatproject.info)

### Installing
Prerequisites:
```
sudo apt-get install figlet cmake libssl-dev libcurl4-gnutls-dev libglib2.0-dev libgsl0-dev socat
```
Install [LibWebsockets](https://github.com/warmcat/libwebsockets):
```
cd ./libwebsockets
mkdir build
cd build
cmake ..
make
sudo make install
sudo ldconfig
```
Clone this repository
Clone [SEPA-C](https://github.com/arces-wot/SEPA-C)
Run [SEPA](https://github.com/arces-wot/SEPA)

## Run the code
To compile:
```
eval $(cat ridMain.c | grep gcc)
```
See the manpage.txt to know how to run the software.

## Contributing
For SEPA-C apis, see [SEPA](https://github.com/arces-wot/SEPA) and [SEPA-C](https://github.com/arces-wot/SEPA-C)

## Authors
* **Francesco Antoniazzi** - *Coding* - [fr4ncidir](https://github.com/fr4ncidir)
* **Giacomo Paolini** - *Supporter*
