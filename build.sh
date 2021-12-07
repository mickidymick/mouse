#!/bin/bash
gcc -o mouse.so mouse.c $(yed --print-cflags) $(yed --print-ldflags)
