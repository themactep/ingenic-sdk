#!/bin/sh

ARG=$1
num=0

if [ $ARG = "md5" ]
then
	while true
	do
		txt=$(date +%s%B%h%m%s%A)
		str1=$(echo -n $txt | md5sum | awk '{print $1}')
		str2=$(./sample_hash md5 $txt)
		if [ $str1 != $str2 ]
		then
			echo "error!!!!!!!!!!"
			echo "txt: $txt"
			echo "str1: $str1"
			echo "str2: $str2"
			break
		fi

		b=$(($num % 10))
		if [ $b = 0 ]
		then
			echo "Numble of MD5 Tests: $num OK!"
		fi
		num=$(($num + 1))
	done
elif [ $ARG = "sha1" ]
then
	while true
	do
		txt=$(date +%s%B%h%m%s%A)
		str1=$(echo -n $txt | sha1sum | awk '{print $1}')
		str2=$(./sample_hash sha1 $txt)
		if [ $str1 != $str2 ]
		then
			echo "error!!!!!!!!!!"
			echo "txt: $txt"
			echo "str1: $str1"
			echo "str2: $str2"
			break
		fi

		b=$(($num % 10))
		if [ $b = 0 ]
		then
			echo "Numble of SHA1 Tests: $num OK!"
		fi
		num=$(($num + 1))
	done
elif [ $ARG = "sha224" ]
then
	while true
	do

		txt="helloWORLD1234567890"
		str1="cf0779a2503235f49a0514439985bd1117f941198e14a2b161c91ce9"
		str2=$(./sample_hash sha224 $txt)
		if [ $str1 != $str2 ]
		then
			echo "error!!!!!!!!!!"
			echo "txt: $txt"
			echo "str1: $str1"
			echo "str2: $str2"
			break
		fi

		b=$(($num % 10))
		if [ $b = 0 ]
		then
			echo "Numble of SHA224 Tests: $num OK!"
		fi
		num=$(($num + 1))
	done
elif [ $ARG = "sha256" ]
then
	while true
	do
		txt=$(date +%s%B%h%m%s%A)
		str1=$(echo -n $txt | sha256sum | awk '{print $1}')
		str2=$(./sample_hash sha256 $txt)
		if [ $str1 != $str2 ]
		then
			echo "error!!!!!!!!!!"
			echo "txt: $txt"
			echo "str1: $str1"
			echo "str2: $str2"
			break
		fi

		b=$(($num % 10))
		if [ $b = 0 ]
		then
			echo "Numble of SHA256 Tests: $num OK!"
		fi
		num=$(($num + 1))
	done
elif [ $ARG = "sha384" ]
then
	while true
	do
		txt="helloWORLD1234567890"
		str1="37dd1ba7ea912a292b64770b9a9da2b732dfa2c10942281dd420e14b08e55d4984b8501cf7ca6116847fd459d7a99529"
		str2=$(./sample_hash sha384 $txt)
		if [ $str1 != $str2 ]
		then
			echo "error!!!!!!!!!!"
			echo "txt: $txt"
			echo "str1: $str1"
			echo "str2: $str2"
			break
		fi

		b=$(($num % 10))
		if [ $b = 0 ]
		then
			echo "Numble of SHA384 Tests: $num OK!"
		fi
		num=$(($num + 1))
	done
elif [ $ARG = "sha512" ]
then
	while true
	do
		txt=$(date +%s%B%h%m%s%A)
		str1=$(echo -n $txt | sha512sum | awk '{print $1}')
		str2=$(./sample_hash sha512 $txt)
		if [ $str1 != $str2 ]
		then
			echo "error!!!!!!!!!!"
			echo "txt: $txt"
			echo "str1: $str1"
			echo "str2: $str2"
			break
		fi

		b=$(($num % 10))
		if [ $b = 0 ]
		then
			echo "Numble of SHA512 Tests: $num OK!"
		fi
		num=$(($num + 1))
	done
else
	 echo "md5:    ./test.sh md5"
	 echo "sha1:   ./test.sh sha1"
	 echo "sha224: ./test.sh sha224"
	 echo "sha256: ./test.sh sha256"
	 echo "sha384: ./test.sh sha384"
	 echo "sha512: ./test.sh sha512"
fi

