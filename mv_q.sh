#!/bin/bash

mkdir -p .quarantine
mkdir -p .quarantine/"$(dirname "$1")"
mv -T "$1" .quarantine/"$1"

