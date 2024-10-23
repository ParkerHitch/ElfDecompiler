#!/bin/bash

gcc -fno-stack-protector -no-pie -o $1.elf $1.c
