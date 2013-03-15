################################################################################
#
#   LOAD RELEVANT MODULES
#
################################################################################

from optparse import OptionParser
import commands
import os

################################################################################
#
#   USER DEFINED VARIABLES
#
################################################################################

inspinj_seed=7000  ## Your inspinj seed. The inspnest dataseed will be created from this, adding three zeros at the end (e.g. inspinj 7001 --> inspnest 7001000)
type_inj="dphi6"   ## This has to be either GR or the name of the test param (e.g. dphi7)
shift= 1            ## This is in percent. If type_inj is GR this will be ignored (you don't need to set it to zero or empty string)
distr='c'          ## Distribution of the values for the shift. Set to 'c' for constant shift, 'u' for uniform or 'n' for normal
sigma=0.0          ## Sigma for the normal distribution. This is in percent.
number_of_injs=200 ## This is the number of signals created in the xml file. Inspnest will analize all of them.
append_to_database=1 ## If 1 Bayes factors and mediand and stdevs will be appended to the remote database (see below)

approx_for_inj='IMRPhenomFBTestthreePointFivePN' ## This is the WF to use for the inj file. Use the usual lalapps_inspinj sintax here.

remote_script='svitale@login.nikhef.nl:/project/gravwav/Database_files/safe_append.sh' ## This is the remote file which appends to the database. IT SHOULD NOT BE CHANGED! 
remote_database='/project/gravwav/Database_files/%s_BayesFactorsDatabase.txt'%(approx_for_inj)   ## Successful runs are appended to remote_database (will be created in the same folder where safe_append.sh is). Failed runs are appended to 'remote_database'_failed. Median and stdev of the parameters are appended to 'remote_database'_parameters. The first part of the name contains the approximant. Once agreed upon, that should not be changed

if type_inj!='GR' and shift>0 and distr=='c':
    type_name=type_inj+'_'+repr(shift)
elif type_inj!='GR' and shift<0 and distr=='c':
    type_name=type_inj+'_m'+repr(-shift)
elif type_inj!='GR' and not(distr=='c'):
    if distr=='u':
        type_name+='u'
    elif distr=='n':
        if repr(sigma)>0:
            type_name+='pm'+repr(sigma)
        else:
            print "You are using a normal distribution for the shifts, but the stddev sigma seems to be zero or negative! Exiting..."
            exit(1)
    else:
        print "You are using distr=%s. Allowed values are 'c','u' and 'n'. Exiting..."%distr
        exit(1)

type_name+='pc'
if type_inj=='GR':
    type_name=type_inj

PATH_TO_OPT="/home/salvatore.vitale/lalsuite_branches/nikhef/opt"  ## Path to the opt folder of your installation

basefolder = "/home/salvatore.vitale/test_Database/%s/%s"%(type_name,inspinj_seed)                       ##
postprocfolder = "/home/salvatore.vitale/public_html/test_Database/%s/%s"%(type_name,inspinj_seed)       ##
scratchdir = "/scratch/salvatore.vitale/test_Database/%s/%s"%(type_name,inspinj_seed)            ##
logdir = "/people/salvatore.vitale/test_Database/%s/%s"%(type_name,inspinj_seed)                  ## logdir and scratchdir are ignored in all the clusters but UWM.                 

 ## NOTE: You only need to change the path leaving the last two levels are they are (i.e. /%s/%s). The code w#ill add the seed and the type of run (GR or tested param +value of the shift)

hypotheses =["dphi1","dphi2","dphi3","dphi4"]
allPNparams = ["dphi0","dphi1", "dphi2", "dphi3", "dphi4", "dphi5", "dphi6", "dphi7", "dphi8", "dphi9", "spin1z", "spin2z"]

################################################################################
#
#   CREATING THE INJECTION FILE
#
################################################################################

print "Creating the xml file\n"

if not os.path.isdir(basefolder):
    os.makedirs(basefolder)
outname=os.path.join(basefolder,'injections_%s_%s.xml'%(type_name,inspinj_seed))
time_step=1000
gps_start=932170000
gps_end=gps_start+time_step*(number_of_injs )
#gps_end=gps_start+time_step*(number_of_injs +20)

inspinj_command="""%s/lalapps/bin/lalapps_inspinj \
--output %s \
--f-lower 20.0 \
--gps-start-time %s \
--gps-end-time %s \
--seed %s \
--waveform %s \
--min-distance 3.00e+05 \
--max-distance 1.25e+06 \
--d-distr volume \
--l-distr random \
--i-distr uniform \
--min-mass1 5.0 \
--max-mass1 15.0 \
--min-mass2 5.0 \
--max-mass2 15.0 \
--m-distr componentMass \
--min-mtotal 10.0 \
--max-mtotal 30.0 \
--disable-spin \
--amp-order 0 \
--time-step %s"""%(PATH_TO_OPT,outname,gps_start,gps_end,inspinj_seed,approx_for_inj,time_step)

if type_inj!='GR':
    inspinj_command+=""" \
--enable-dphi \
--%s %s"""%(type_inj,repr(shift/100.0)) 
    if distr=='u':
        inspinj_command+=""" \
--uniform-dphi"""
    elif distr=='n':
        inspinj_command+=""" \
--s%s %s"""%(type_inj,repr(sigma/100.0))    
    
print inspinj_command
os.system(inspinj_command)
os.system('seed_line=`cat '+outname+' | grep seed`;temp=`echo ${seed_line%\\"*}`;temp2=`echo ${temp##*\\"}`;echo "The file was created with seed $temp2"')
print "\n"
################################################################################
#
#   SETS INSPNEST DATASEED AND XMLNAME
#
################################################################################

inspnest_dataseed=str(inspinj_seed)+"000"
xmlname = outname

################################################################################
#
#   ARGUMENT PARSING
#
################################################################################
"""
parser = OptionParser()
parser.add_option("-i", "--injectionxml", dest="xmlname", help="Injection xml filename", metavar="FILE")
parser.add_option("-b", "--basefolder", dest="basefolder", metavar="FOLDER", help="output folder NS")
parser.add_option("-p", "--postprocfolder", dest="postprocfolder", metavar="FOLDER", help="post processing folder")
parser.add_option("-a", "--parser", dest="parsername", metavar="FILE", help="parser filename (default=parser.ini)", default="parser.ini")

(options, args) = parser.parse_args()

xmlname = options.xmlname
basefolder = options.basefolder
postprocfolder = options.postprocfolder
parsername = options.parsername
"""

if basefolder[-1] != "/":
	basefolder += "/"

if postprocfolder[-1] != "/":
	postprocfolder += "/"


################################################################################
#
#   DEFINE USEFUL FUNCTIONS
#
################################################################################

def combinations(iterable, r):
    # combinations('ABCD', 2) --> AB AC AD BC BD CD
    # combinations(range(4), 3) --> 012 013 023 123
    pool = tuple(iterable)
    n = len(pool)
    if r > n:
        return
    indices = range(r)
    yield tuple(pool[i] for i in indices)
    while True:
        for i in reversed(range(r)):
            if indices[i] != i + n - r:
                break
        else:
            return
        indices[i] += 1
        for j in range(i+1, r):
            indices[j] = indices[j-1] + 1
        yield tuple(pool[i] for i in indices)

def createCombinationsFromList(list):
	outputlist = []
	outputlist.append("GR")
	for L in xrange(len(list)):
		for subset in combinations(list, L+1):
			"""
			temp = ""
			for subsetitem in subset:
				temp += subsetitem
			"""
			outputlist.append(subset)

	return outputlist

def ensure_dir(f):
	#d = os.path.dirname(f)
	if not os.path.exists(f):
			os.makedirs(f)


################################################################################
#
#   CREATE ALL POSSIBLE COMBINATIONS FROM A LIST
#
################################################################################

curdir = os.getcwd()
foldernames=""
parser_paths=""

allcombinations = createCombinationsFromList(hypotheses)

for run in allcombinations:

	# GETTING CORRECT PINPARAMS
	pinparams = ""
	for item in allPNparams:
		if item not in run:
			if pinparams == "":
				pinparams += item
			else:
				pinparams += ","+item
	# DETERMINE FOLDER NAME
	foldername = ""
	for item in run:
		foldername += item
	ensure_dir(basefolder+foldername)
	os.chdir(basefolder+foldername)

	parser_text ="[analysis]"

	parser_text += \
	"""
padding=1
psd-chunk-length=20.0
nlive=1000
nmcmc=100
nparallel=1
ifos=['H1','L1','V1']
#events=all
events=[0:"""+str(number_of_injs-1)+"]"+"""
seed=1
data_seed="""+str(inspnest_dataseed)+"""
analysis-chunk-length=20.0

[condor]
inspnest=PATH_TO_OPT/lalapps/bin/lalapps_inspnest
combinez=PATH_TO_OPT/lalapps/bin/lalapps_combine_evidence
datafind=PATH_TO_OPT/glue/bin/ligo_data_find
mergescript=PATH_TO_OPT/lalapps/bin/lalapps_merge_nested_sampling_runs
resultspage=PATH_TO_OPT/pylal/bin/cbcBayesPostProc.py
segfind=PATH_TO_OPT/glue/bin/ligolw_segment_query
ligolw_print=PATH_TO_OPT/glue/bin/ligolw_print
coherencetest=PATH_TO_OPT/lalapps/bin/lalapps_coherence_test
database=PATH_TO_OPT/pylal/bin/AppendToDatabase.py

[datafind]
url-type=file

[data]
types=['LALAdLIGO','LALAdLIGO','LALAdVirgo']
channels=['LALAdLIGO','LALAdLIGO','LALAdVirgo']

[inspnest]
dt=0.1
srate=4096.0
approximant=IMRPhenomFBTest

[inspnest_all]
dmin=1.0
dmax=1500.0
deta=0.1
flow=20.0
amporder=0
phaseorder=7
"""
	parser_text +="pinparams=" + pinparams

	parser_text += \
	"""

[results]
skyres=1.0
"""

	parser_text += "basedir="+postprocfolder+foldername
	parser_text += \
	"""

[segfind]
segment-url=https://segdb.ligo.caltech.edu

[segments]
l1-analyze = L1:DMT-SCIENCE:2
h1-analyze = H1:DMT-SCIENCE:2
v1-analyze = V1:ITF_SCIENCEMODE:6

"""

	parser_fileobject = open("parser_"+foldername+".ini","w")
        parser_text=parser_text.replace('PATH_TO_OPT',PATH_TO_OPT)
        parser_fileobject.write(parser_text)
	parser_fileobject.close()
        foldernames+=str(foldername)+" "
	parser_paths+=str(os.path.join(basefolder,foldername,"parser_"+foldername+".ini"))+" "

logd="None"
scrd="None"
        
for i in os.uname():
    if i.find("uwm")!=-1:
        logd=logdir
        scrd=scratchdir
#print PATH_TO_OPT + "/lalapps/bin/lalapps_nest_multi_parser -i "+ parser_paths + " -I "+outname+ " -r " + basefolder +" -P "+foldernames+" -p " + logd + " -l " + scrd + " -R "+remote_script+" -S "+type_name+" -Q "+str(inspinj_seed)+" -D "+remote_database
#exit(1)
#
if append_to_database==1:
    os.system(PATH_TO_OPT + "/lalapps/bin/lalapps_nest_multi_parser_reduced -i "+ parser_paths + " -I "+outname+ " -r " + basefolder +" -P "+foldernames+" -p " + logd + " -l " + scrd + " -R "+remote_script+" -S "+type_name+" -Q "+str(inspinj_seed)+" -D "+remote_database)
elif append_to_database==0:
    os.system(PATH_TO_OPT + "/lalapps/bin/lalapps_nest_multi_parser_reduced -i "+ parser_paths + " -I "+outname+ " -r " + basefolder +" -P "+foldernames+" -p " + logd + " -l " + scrd )
else:
    print "You did not specify a value for the variable append_to_database (allowed: 0 -> turn off, 1-> turn on). Exiting..."
    exit(1)
# RETURN TO CURRENT WORKING DIRECTORY
os.chdir(curdir)