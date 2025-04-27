#!/bin/bash

# This script fetches the current list of trusted CAs blessed by Mozilla
# for web tls validation, and processes it into two outputs
#
# - ./trust/webroot/* consisting of ./_trust/webroot/der  a static, serveable set
#     of trusted DER certs, with symlinks in ./_trust/webroot/by-skid and
#     ./_trust/webroot/by-iss allowing serving the DER matching a given
#     SubjectKeyIdentifier or Issuer + serial combination (suitably encoded)
#
# - ./_trust/blob-XXXX.bin  a single blob containing indexes and DER CA certs
#
# - ./_trust/trust_blob.h   a C uint8_t array formatted copy of blob-XXXX.bin

# The trust blob layout is currently
#
# 54 42 4c 42     Magic "TBLB"
# 00 01           MSB-first trust blob layout version
# XX XX           MSB-first count of certificates
# XX XX XX XX     MSB-first trust blob generation unix time
# XX XX XX XX     MSB-first offset of cert length table (MSB-first 16-bit length-per-cert)
# XX XX XX XX     MSB-first offset of SKID length table (8-bit length-per-cert)
# XX XX XX XX     MSB-first offset of SKID table
# XX XX XX XX     MSB-first total blob length
#
# XX .. XX        DER certs (start at +0x1c)
# XX .. XX        DER cert length table (MSB-first 16-bit per cert)
# XX .. XX        SKID length table (8-bit per cert)
# XX .. XX        SKID table (variable per cert)
#

echo "Mozilla trust bundle for TLS validation processing  Andy Green <andy@warmcat.com>"
echo

rm -rf _trust
mkdir _trust

wget -O _trust/trusted.txt "https://ccadb-public.secure.force.com/mozilla/IncludedRootsPEMTxt?TrustBitsInclude=Websites"
#cp ~/Downloads/IncludedRootsPEM.txt _trust/trusted.txt

if [ $? -ne 0 ]; then
	echo "Failed to get current website trust bundle"
	exit 1
fi

mkdir -p _trust/webroot/by-skid _trust/webroot/by-iss _trust/webroot/der

echo 0 > _trust/ofs
echo 0 > _trust/count
echo 0 > _trust/skidtab

GT=`date +%s`
BN=_trust/blob-$GT.bin

cat _trust/trusted.txt | while read _line ; do
	line=`echo -n $_line | sed 's/\r$//g'`
	if [ "$line" == "-----BEGIN CERTIFICATE-----" ] ; then
		echo $line > _trust/single
	else
		echo $line >> _trust/single

		if [ "$line" == "-----END CERTIFICATE-----" ] ; then
			openssl x509 -in _trust/single -text -noout > _trust/c1
			if [ $? -ne 0 ] ; then
				echo "FAILED"
				exit 1
			fi

			ISS=`cat _trust/c1 | grep Issuer: | sed "s/.*://g" | sed "s/^\ *//g"`
			SER=`cat _trust/c1 | grep "Serial Number:" | sed "s/.*://g" | sed "s/^\ *//g" | sed "s/\ .*//g"`
			if [ -z "$SER" ] ; then
				SER=`cat _trust/c1 | sed -e "1,/.*Serial Number:/ d" | head -n 1 | sed "s/^\ *//g" | sed "s/\ .*//g"`
			fi
			SKID=`cat _trust/c1 | sed -e '1,/.*X509v3 Subject Key Identifier:/ d' | sed -n '/Signature.*/q;p' | \
				grep ':' | grep -v ': ' | grep -v ':$' | grep -v U | grep -v k | grep -v T | grep -v "i" | \
				grep -v "S" | grep -v "V" | sed "s/^\ *//g"`
			SKID_NO_COLONS=`echo -n $SKID | sed "s/://g"`

			na=`cat _trust/c1 | grep "Not\ After\ :" | sed "s/.*\ :\ //g"`
			ct=`date +%s`
			ts=`date --date="$na" +%s`
			life_days=`echo -n "$(( ( $ts - $ct ) / 86400 ))"`

			echo "$life_days $safe" >> _trust/life
			if [ $life_days -lt 1095 ] ; then
				echo "$life_days $safe" >> _trust/life_lt_3y
			fi

			echo "issuer=\"$ISS\", serial=\"${SER^^}\", skid=\"${SKID_NO_COLONS^^}\", life_days=\"${life_days}\""

			issname=`echo -n "$ISS"_"$SER" | tr -cd '[a-zA-Z0-9]_'`
			skidname=`echo -n "$SKID_NO_COLONS" | tr -cd '[a-zA-Z0-9]_'`
			safe=$issname"_"$skidname

			cat _trust/single | grep -v -- '---' | base64 -d > _trust/webroot/der/$safe
			cd _trust/webroot/by-skid
			ln -sf ../der/$safe $SKID_NO_COLONS
			cd ../../..
			cd _trust/webroot/by-iss
			ln -sf ../der/$safe $issname
			cd ../../..

			DERSIZ=`cat _trust/single | grep -v -- '---' | base64 -d | wc -c | cut -d' ' -f1`

			cat _trust/single | grep -v -- '---' | base64 -d | hexdump -C | tr -s ' ' | sed 's/\ $//g' | \
				cut -d' ' -f 2-17 | cut -d'|' -f1 | grep -v 000 | sed "s/\ //g" | sed ':a;N;$!ba;s/\n//g' | xxd -r -p >> _trust/_ders

			printf "%04x" $DERSIZ | xxd -r -p  >> _trust/_derlens

echo $SKID

			if [ ! -z "$SKID" ] ; then
				echo -n "$SKID_NO_COLONS" | xxd -r -p >> _trust/_skid
			fi
			SKIDSIZ=`echo -n $SKID_NO_COLONS | xxd -r -p | wc -c | cut -d' ' -f1`
			printf "%02x" $SKIDSIZ | xxd -r -p  >> _trust/_skidlens

			OFS=`cat _trust/ofs`
			echo -n $(( $OFS + $DERSIZ )) > _trust/ofs
			COUNT=`cat _trust/count`
			echo -n $(( $COUNT +1 )) > _trust/count
			ST=`cat _trust/skidtab`
			echo -n $(( $ST + ( `echo -n $skidname | wc -c | cut -d' ' -f1` / 2 ) )) > _trust/skidtab

			rm -f _trust/single

		fi
	fi

done

	COUNT=`cat _trust/count`
	OFS=`cat _trust/ofs`
	ST=`cat _trust/skidtab`

	# everything in the layout framing is MSB-first

	# magic
	echo -n "TBLB" > $BN
	# blob layout version
	echo -n 0001 | xxd -r -p >> $BN
	# number of certs in the blob
	printf "%04x" $COUNT | xxd -r -p >> $BN
	# unix time blob was created
	printf "%08x" $GT | xxd -r -p >> $BN

	POS=28
	POS=$(( $POS + `cat _trust/_ders | wc -c | cut -d' ' -f1` ))

	# blob offset of start of cert length table
	printf "%08x" $POS | xxd -r -p >> $BN

	POS=$(( $POS + `cat _trust/_derlens | wc -c | cut -d' ' -f1` ))

	# blob offset of start of SKID length table
	printf "%08x" $POS | xxd -r -p >> $BN

	POS=$(( $POS + `cat _trust/_skidlens | wc -c | cut -d' ' -f1` ))

	# blob offset of start of SKID table
	printf "%08x" $POS | xxd -r -p >> $BN

	POS=$(( $POS + `cat _trust/_skid | wc -c | cut -d' ' -f1` ))

	# blob total length
	printf "%08x" $POS | xxd -r -p >> $BN


	# the DER table, start at +0x1c
	cat _trust/_ders >> $BN
	# the DER length table
	cat _trust/_derlens >> $BN
	# the SKID length table
	cat _trust/_skidlens >> $BN
	# the SKID table
	cat _trust/_skid >> $BN

# produce a C-friendly version of the blob

	cat $BN | hexdump -v -C | tr -s ' ' | sed 's/\ $//g' | \
		cut -d' ' -f 2-17 | cut -d'|' -f1 | grep -v 000 | sed "s/\ /,\ 0x/g" | sed "s/^/0x/g" | \
		sed 's/\, 0x$//g' | sed 's/$/,/g' >> _trust/trust_blob.h


	echo
	echo "$COUNT CA certs, $POS byte blob"
	echo
	echo "CAs expiring in less than 3 years (days left):"
	sort -h _trust/life_lt_3y

	rm -f _trust/count _trust/_idx _trust/_idx_skid _trust/ofs _trust/_skid _trust/skidtab _trust/life _trust/life_lt_3y _trust/c1 _trust/single _trust/_derlens _trust/_ders _trust/_skid _trust/_skidlens

exit 0

