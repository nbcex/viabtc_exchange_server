# 1 once
## 1.1 install depenency
```shell
sudo apt-get install -y libev-dev libssl-dev libmpdec-dev libjansson-dev  libmysqlclient-dev liblz4-dev
```

## 1.2 untar librdkafka & curl 
```shell
(cd depends && tar -xzf v0.11.6.tar.gz && ln -sf librdkafka-0.11.6 librdkafka)
(cd depends && tar -xzf curl-7.57.0.tar.gz && ln -sf curl-curl-7_57_0 curl)
```

## 1.3 compile depends
```shell
(cd depends/curl && ./buildconf && ./configure --disable-shared --enable-static --without-libidn --without-ssl --without-librtmp --without-gnutls --without-nss --without-libssh2 --without-zlib --without-winidn --disable-rtsp --disable-ldap --disable-ldaps --disable-ipv6 && make)
(cd depends/librdkafka && ./configure && make)
(cd depends/hiredis  && make)
```

# 2 make
