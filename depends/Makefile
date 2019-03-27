all: apt hiredis curl rdkafka
apt:
	sudo apt-get update && sudo apt-get install -y libev-dev libssl-dev libmpdec-dev libjansson-dev  libmysqlclient-dev liblz4-dev
curl:
	tar -xzf curl-7.57.0.tar.gz && ln -sf curl-curl-7_57_0 curl
	cd curl && ./buildconf && ./configure --disable-shared --enable-static --without-libidn --without-ssl --without-librtmp --without-gnutls --without-nss --without-libssh2 --without-zlib --without-winidn --disable-rtsp --disable-ldap --disable-ldaps --disable-ipv6 && make
rdkafka:
	tar -xzf v0.11.6.tar.gz && ln -sf librdkafka-0.11.6 librdkafka
	cd librdkafka && ./configure && make
hiredis:
	cd hiredis && make