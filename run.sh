#!/bin/sh
export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
tcc `pkg-config --static --libs glfw3` -lvulkan `pkg-config --cflags freetype2` -lfreetype main.c -run
