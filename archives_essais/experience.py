import essai_pb2
import nouveau_pb2

m_ancien = essai_pb2.Message()
m_ancien.texte = "bonjour"
m_ancien.valeur = 42
octets = m_ancien.SerializeToString()
print("Octets envoyés par l'ANCIEN dictionnaire :", octets)

m_nouveau = nouveau_pb2.Message()
m_nouveau.ParseFromString(octets)
print("Valeur relue par le NOUVEAU dictionnaire :", m_nouveau.valeur)