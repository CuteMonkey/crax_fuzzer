from rlpy.Domains.Domain import Domain
import numpy
import re
import itertools as IT

class PC_states(Domain):
    #The path to directory storing raw data.
    raw_data_path = '/home/chengte/SCraxF/RL_codes/raw_data'
    #The file name of infomation of path constraints.
    pc_info_name = 'info_collection.txt'
    #The file name of result of PC combinations.
    pc_result_name = 'try_record.txt'
    
    def __init__(self, df=0.9):
        '''
        param: df: discount factor
        '''
        #The number of path constraints.
        self.pc_num = self._get_pc_num()
        #The max length of a combination of PCs.
        self.max_length = self._get_max_length()
        #The dict to record the map that index into combination.
        self.combination_table = {}
        self._set_combination_table()
        #The total number of PC combinations states.
        self.pc_state_num = len(self.combination_table)
        #The dict used to store PC results.
        self.pc_results = [0] * self.pc_state_num
        self._set_result()
        
        self.discount_factor = df
        self.actions_num = self.pc_state_num
        self.statespace_limits = numpy.array([[0, self.pc_state_num]])
        #CPC: Combinations of Path Constraints
        self.DimNames = ['CPCs']
        self.episodeCap = self.max_length * 100
        
        super(PC_states, self).__init__()
    
    def s0(self):
        self.state = numpy.array([1])
        return self.state, False, self.possibleActions()
    
    def step(self, a):
        r = self._get_reward(self.state)
        next_state = numpy.array([a])
        self.state = next_state
        return r, next_state, self.isTerminal(), self.possibleActions()
    
    def isTerminal(self):
        result = self.pc_results[self.state[0]]
        if result == 4:
            return True
        else:
            return False
    
    #Get pc_num from pc info file.
    def _get_pc_num(self):
        pc_info_file = open(self.raw_data_path + '/' + self.pc_info_name, 'r')
        pc_n = 0
        first_line = pc_info_file.readline()
        if first_line != '':
            match = re.search('(\d+)', first_line)
            if match != None:
                pc_n = match.group(1)
        pc_info_file.close()
        return int(pc_n)
    
    #Get reward from verify result file.
    def _get_reward(self, s):
        result = self.pc_results[s[0]]
        if result == 1:
            return -5
        elif result == 2:
            return -1
        elif result == 3:
            return -2
        elif result == 4:
            return 10
        else: #result == 0
            return 0
    
    #Get the max number of PC combination.
    def _get_max_length(self):
        result_file = open(self.raw_data_path + '/' + self.pc_result_name, 'r')
        lines = result_file.readlines()
        last_line = lines[-1]
        result_file.close()
        match = re.match('PC(.+):', last_line)
        line = match.group(1)
        line = line.split(' ')
        result = 0
        for pc_str in line:
            if pc_str != '':
                result += 1
        return result
   
    #Set the content of combination table.
    def _set_combination_table(self):
        all_pc_list = []
        for i in range(self.pc_num):
            all_pc_list.append(i + 1)
        all_pc_tuple = tuple(all_pc_list)
        
        cur_combination_len = 0
        cur_index = 0
        while cur_combination_len <= self.max_length:
            for tuple_e in IT.combinations(all_pc_tuple, cur_combination_len):
                self.combination_table[cur_index] = tuple_e
                cur_index += 1
            cur_combination_len += 1
     
    #Set the results of pc from pc reuslt file.
    def _set_result(self):
        result_file = open(self.raw_data_path + '/' + self.pc_result_name, 'r')
        line = result_file.readline()
        a_result = 0
        cur_index = 1
        while line != '':
            a_result = int(line[-2])
            if line[0:3] == 'ALL':
                line = result_file.readline()
                continue
            elif line[0:4] == 'NULL':
                self.pc_results[0] = a_result
            else:
                self.pc_results[cur_index] = a_result
                cur_index += 1
            line = result_file.readline()
        result_file.close()
