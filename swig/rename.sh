#!/bin/sh

echo $* | sed -e 's/model_//' -e 's/rhp_//' -e 's/reshop_//' -e 's/ctx_/mdl_/'
