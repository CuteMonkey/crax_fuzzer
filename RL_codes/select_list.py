def int_to_list(i):
    '''
    This function convert int to select list
    according to 1s' position of binary presentation.
    The smallest selection index is 1.
    '''
    result = []
    bin_i = bin(i)
    bin_i = bin_i[2:]
    bin_i = bin_i[::-1]
    s_index = 1
    for b in bin_i:
        if b == '1':
            result.append(s_index)
        s_index += 1
    return result

def list_to_int(ls):
    '''
    The reverse operaton of function
    int_to_list().
    '''
    result = 0
    for e in ls:
        result += pow(2, e - 1)
    return result

def int_to_str(i):
    '''
    This function convert the result of
    int_to_list() into string.
    '''
    ls = int_to_list(i)
    result = list_to_str(ls)
    return result

def list_to_str(ls):
    '''
    This function convert the content of
    list parameter into string by concatenating its
    elements and dividing by space.
    '''
    result = ''
    for x in ls:
        result += str(x)
        result += ' '
    result = result[:-1]
    return result

def str_to_list(ls_str):
    '''
    The reverse operation of function
    list_to_str().
    '''
    ints_str_ls = ls_str.split(' ')
    result = []
    for int_str in ints_str_ls:
        result.append(int(int_str))
    return result
