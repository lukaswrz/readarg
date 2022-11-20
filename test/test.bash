#!/usr/bin/env bash

cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null

./test \
	-e a \
	--expr b \
	c \
	-xd \
	e \
	--expression=f \
	g \
	-c h \
	--config=i \
	--uri j \
	k \
	-il \
	-b m \
	--backup n \
	--backup-file=o \
	p \
	q \
	-vv \
	r \
	-v \
	-s \
	s \
	t \
	--sort \
	u \
	v \
	w \
	-- \
	x \
	y \
	z

./test -e a b c --help

./test -e a b c --version
