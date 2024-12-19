#!/bin/bash

num=0
function printRequestNum() {
    if [[ $num -ne 0 ]]
        then printf "\n\n" ;
    fi
    num=$((num + 1))

    printf "Request %d\n" $num
}

printRequestNum

curl --header "Content-Type: application/json" \
  --request PATCH \
  --data '{"username":"german","password":"datarozhdenia"}' \
  http://localhost:10000/

printRequestNum

curl --header "Content-Type: application/json" \
  --request GET \
  --data '{"username":"xyz"}' \
  http://localhost:10000/

printRequestNum

curl --header "Content-Type: application/json" \
  --request PATCH \
  --data '{"username":"vasya","ocenka":"3.5"}' \
  http://localhost:10000/

printRequestNum

curl --header "Content-Type: application/json" \
  --request GET \
  --data '{"username":"xyz"}' \
  http://localhost:10000/

printf "\n"
