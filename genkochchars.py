#! env python

import sys
import numpy
import random

koch_chars = 'KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X'
# How much the already learned characters will loose in importance
decay_ratio = 2/3.;
if len(sys.argv) < 3:
    print 'need character number and level as parameter'
    exit (-1)
group_number = int(sys.argv[1])
level = int(sys.argv[2])
print 'level:', level
probabilities=[1/2., 1/2.]
for i in range(1,level):
    probabilities = [p * decay_ratio for p in probabilities]
    probabilities.append(1-decay_ratio)
#print probabilities
prob_cumsum = numpy.cumsum(probabilities)
#print prob_cumsum
for i in range(0,group_number):
    group_len = random.randrange(2,6)
    #print "group length:", group_len
    for j in range(group_len):
        rand_value = random.random()
        #print "  rand:", rand_value
        for k in range(len(prob_cumsum)):
            #print "    k", k, "->", koch_chars[int(k)]
            if rand_value < prob_cumsum[k]:
                sys.stdout.write (koch_chars[int(k)])
                break
    sys.stdout.write(' ')
sys.stdout.write('\n')            
