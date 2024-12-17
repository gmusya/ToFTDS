#!/bin/bash

curl --header "Content-Type: application/json" \
  --request PATCH \
  --data '{"username":"xyz","password":"xyz"}' \
  http://localhost:10000/

curl --header "Content-Type: application/json" \
  --request GET \
  --data '{"username":"xyz","password":"xyz"}' \
  http://localhost:10000/
