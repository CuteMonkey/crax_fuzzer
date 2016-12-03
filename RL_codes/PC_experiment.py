from rlpy.Agents import Q_Learning
from PC_states import PC_states
from PC_features import PC_features
from rlpy.Policies import eGreedy
from rlpy.Experiments import Experiment
import sys

def make_experiment(exp_id, path, pc_level_sz):
    opt = {}
    opt['exp_id'] = exp_id
    opt['path'] = path
    
    domain = PC_states()
    opt['domain'] = domain
    
    representation = PC_features(domain, pc_level_sz)
    policy = eGreedy(representation, epsilon=0.2)
    
    opt['agent'] = Q_Learning(policy, representation, domain.discount_factor, 0.0,
                        initial_learning_rate=0.3,
                        learning_rate_decay_mode='boyan',
                        boyan_N0=100)
    opt['checks_per_policy'] = 50
    opt['max_steps'] = 2000
    experiment = Experiment(**opt)
    return experiment

result_path = '/home/chengte/SCraxF/RL_codes/result'
exp_id, pc_level_sz = 0, 0
if len(sys.argv) > 2:
    exp_id = int(sys.argv[1])
    pc_level_sz = int(sys.argv[2])
else:
    exp_id = 1
    pc_level_sz = 5
experiment = make_experiment(exp_id, result_path, pc_level_sz)
experiment.run()
experiment.save()

#the name of file which records the feature weights
fw_file_name = 'feature_weight{:0>3}.txt'.format(exp_id)
feature_num = experiment.agent.representation.features_num
action_num = experiment.domain.actions_num
#use to record the number of nonzero grids
nonzero_num = [0] * feature_num
#use to store the sum of grids of each feature
feature_sum = [0.0] * feature_num
i, j, v = 0, 0, 0.0
while i < action_num:
    while j < feature_num:
        v = experiment.agent.representation.weight_vec[feature_num * i + j]
        if v != 0.0:
            nonzero_num[j] += 1
            feature_sum[j] += v
        j += 1
    i += 1
    j = 0
fw_file = open(result_path + '/' + fw_file_name, 'w')
fw_file.write(str(experiment.domain.max_length) + ' ')
fw_file.write(str(pc_level_sz) + '\n')
i = 0
while i < feature_num:
    fw_file.write(str(feature_sum[i] / nonzero_num[i]))
    if i != feature_num - 1:
        fw_file.write('\n')
    i += 1
fw_file.close()
