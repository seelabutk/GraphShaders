#! python3 

with open('/home/pprovins/fg/layout-tools/layouts/SO-Answers/nodes.csv', 'r') as f:
    with open('/home/pprovins/fg/layout-tools/layouts/SO-Answers/nodes_patch.csv', 'w') as o:
        for line in f.readlines():
            if len(line.split(' ')) > 2:
                line = '{},{}'.format(float(line.split(' ')[1]), float(line.split(' ')[2]))
            line.rstrip()
            print(line, file=o)

