#!/bin/sh
glslc -fshader-stage=vert vert.glsl -o vert.spv
glslc -fshader-stage=frag frag.glsl -o frag.spv
