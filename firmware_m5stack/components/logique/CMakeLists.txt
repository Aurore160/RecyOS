idf_component_register(SRCS "logique.c" "tri.c" "niveau.c" "presence.c"
                       INCLUDE_DIRS "include"
                       REQUIRES contrat transport drivers securite)
