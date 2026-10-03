from cobs import cobs

# Un message qui contient volontairement un octet 0x00 au milieu,
# pour simuler une trame Protobuf qui pourrait, par hasard,
# contenir cet octet "interdit".
donnees = b'bonjour\x00monde'
print("Données brutes :", donnees)

# ENCODAGE : COBS transforme ça pour qu'il n'y ait plus AUCUN
# 0x00 à l'intérieur. Le 0x00 devient alors utilisable comme
# marqueur de fin de trame, sans ambiguïté.
encode = cobs.encode(donnees)
print("Après encodage COBS :", encode)
print("Contient un 0x00 ?", b'\x00' in encode)

# La vraie trame envoyée sur le fil ajoute un 0x00 au début ET
# à la fin, comme délimiteurs.
trame = b'\x00' + encode + b'\x00'
print("Trame complète à envoyer :", trame)

# DÉCODAGE : on fait l'inverse pour retrouver les données d'origine.
recupere = cobs.decode(encode)
print("Données récupérées :", recupere)
print("Identique à l'original ?", recupere == donnees)