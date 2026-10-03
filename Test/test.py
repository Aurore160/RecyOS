import ancien_essai_pb2   # on importe le code généré tout à l'heure

# 1. On crée un message, en remplissant les champs définis dans le .proto
m = ancien_essai_pb2.Message()
m.texte = "bonjour depuis le RPi"
m.valeur = 42

print("Avant envoi :", m)

# 2. On le transforme en une suite d'octets, prête à être envoyée en UART
octets = m.SerializeToString()
print("Octets à envoyer :", octets)
print("Nombre d'octets :", len(octets))

# 3. On simule la réception : on reconstruit un message à partir des octets
m2 = ancien_essai_pb2.Message()
m2.ParseFromString(octets)

print("Après réception :", m2)
print("Texte reçu :", m2.texte)
print("Valeur reçue :", m2.valeur)
print("Champ urgent (jamais défini) :", m2.urgent)