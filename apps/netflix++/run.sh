home="/home/haijieg"
glhome="$home/graphlab/graphlab2.2"
graph="/data/netflix/features/splits"
# graph="/data/netflix/features/joined_edge_list.csv"
# graph="/data/netflix/features/test.csv"
mvname="/data/netflix/meta/moviename_escaped.txt"
genre="/data/netflix/features/feat_netflix_genre.csv"
topics="/data/netflix/features/feat_netflix_topic.csv"
bin="$glhome/release/apps/netflix++/netflix_main"

  # --movielist=$mvname \
  # --genre_feature=$genre \
$bin --matrix=$graph \
  --topic_feature=$topics \
  --truncate=false \
  --D=100 \
  --use_bias=false \
  --use_local_bias=false \
  --use_feature_weights=true \
  --lambda=1 \
  --lambda2=0.001 \
  --use_als=true \
  --use_feature_latent=false \
  --max_iter=10 \
  --interactive=false \
  --saveprefix="output/result"
