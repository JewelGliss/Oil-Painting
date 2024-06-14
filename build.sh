#!/usr/bin/env bash

gcc \
    `pkg-config --cflags x11 xi glew` \
    -o output \
    main.c \
    `pkg-config --libs x11 xi glew`\
    -lm -lX11 -lGL
