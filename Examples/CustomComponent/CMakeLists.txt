# StarRating.h/.cpp are the custom component: they include Carbon/Extension.h and nothing else of Carbon, which
# the PublicApiBoundary test enforces.
carbon_add_backend_examples(CustomComponent SOURCES Main.cpp StarRating.cpp StarRating.h)
