#!/bin/sh
tcc `pkg-config --static --libs glfw3` -lvulkan main.c -DNDEBUG
