require("lattice")
require("RMySQL")


p<-function(...) {
	return(paste(..., sep=""))
	}

source("params.R")

fn<-p(prefix, "coinc1", suffix)
fnout<-p(fn)

NAStrings<-c("NA", "NaN", "NAN")

vn<-function(field, i, position) {
	return(data[,p(var_prefix, field, ".", i, ".", position)])
	}

ofn<-function(name) {
	return(file(paste(output_dir, "/", name, sep=""), open="wt"))
	}

col_names<-read.table(pipe("grep data_log 0/*/powerflux.log | head --lines=1"), header=FALSE, sep=" ")

cat("Loading data from ", fn, "\n", sep="")
# Cheat - get a small sample first and use it to find out types and such.
header<-read.table(pipe(p("head --lines=10001 ", fn)), header=FALSE, na.strings=NAStrings, sep=" ")
cat("Found header with", dim(header)[2], "columns\n")
colnames(header)<-unlist(col_names[1, 2:dim(col_names)[2]])

Types<-lapply(header, class)
Types<-lapply(Types, function(x)gsub("logical", "numeric", x))


CreateQuery<-p("CREATE TABLE ", CoincTableName, "(Line INTEGER AUTO_INCREMENT")
LoadQuery<-p("LOAD DATA LOCAL INFILE '", fnout, "' INTO TABLE ", CoincTableName, " FIELDS TERMINATED BY ' ' OPTIONALLY ENCLOSED BY '\"' LINES TERMINATED BY '\n' (")

# "

for(i in 1:length(Types)) {
	Col<-names(Types)[i]

	Name<-Col

	Name<-gsub("\\.", "_", Name)

	CreateQuery<-p(CreateQuery, ", `", Name, "` ", switch(Types[[i]], integer="INTEGER", factor="VARCHAR(10000)", numeric="DOUBLE", "NA"))
	LoadQuery<-p(LoadQuery, ifelse(i==1, "`", ", `"), Name, "`")
	}
CreateQuery<-p(CreateQuery, p(", PRIMARY KEY (Line))"))
cat(CreateQuery, "\n")
LoadQuery<-p(LoadQuery, ")")

#cat("Preprocessing input data (NaN -> NULL)\n")
#system(p("sed 's/[nN][aA][nN]/NULL/g' stats/stat.dat > ", fnout))

cat("Connecting to the database\n")
con<-dbConnect(dbDriver("MySQL"), host=MYSQL_HOST, user=MYSQL_USER, password="", dbname=MYSQL_DB)

cat("Dropping table", CoincTableName, "\n")
try(dbGetQuery(con, p("DROP TABLE ", CoincTableName)), silent=TRUE)

cat("Declaring table", CoincTableName, "\n")
dbGetQuery(con, CreateQuery)

cat("Loading table", CoincTableName, "\n")
dbGetQuery(con, LoadQuery)


cat("Adding index to table", CoincTableName, "\n")
dbGetQuery(con, p("ALTER TABLE `", CoincTableName, "` ADD INDEX (first_bin)"))


cat("Warnings:\n")
print(dbGetQuery(con, "SHOW WARNINGS"))

#Types<-lapply(Types, function(x) {
#	if(x=="factor")return("character")
#	return(x)
#	})
#FieldsUnused<-setdiff(names(Types), FieldsUsed)
#Types[FieldsUnused]<-NULL



