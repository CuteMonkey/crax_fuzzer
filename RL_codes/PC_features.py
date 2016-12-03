from rlpy.Representations.Representation import Representation
import numpy
import math

class PC_features(Representation):
    def __init__(self, domain, level_sz=5):
        self.isDynamic = False
        self.level_size = level_sz
        self.features_num = int(math.ceil(float(domain.pc_num) / self.level_size))
        super(PC_features, self).__init__(domain)
    
    #If the state contains PC index i then has feature index i-1.
    def phi_nonTerminal(self, s):
        f_vec = numpy.zeros(self.features_num, int)
        pcl = self.domain.combination_table.get(s[0])
        for pc in pcl:
            f_vec[int(math.ceil(float(pc) / self.level_size)) - 1] += 1
        return f_vec
     
    def featureType(self):
        return int
