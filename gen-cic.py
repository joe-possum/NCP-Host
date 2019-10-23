fh = open("cic.txt","r")
dict = {}
max = 0
while 1 :
    line = fh.readline()
    if 0 == len(line) :
        break
    tokens = line.split()
    from_decimal = int(tokens[0],10)
    if tokens[1][:2] != "0x" :
        raise RuntimeError("Illegal line: %s"%(line))
    from_hex = int(tokens[1][2:],16)
    if from_hex != from_decimal :
        raise RuntimeError("Value mismatch: %s"%(line))
    if from_hex > max :
        max = from_hex
    if len(tokens) < 3 :
        raise RuntimeError("Missing name? %s"%(line))
    name = tokens[2]
    for word in tokens[3:] :
        name += " " + word
    tokens = name.split('"')
    if len(tokens) > 1 :
        iname = name
        name = ""
        for i in range(len(iname)) :
            if iname[i] == '"' :
                name += '\\"'
            else :
                name += iname[i]
    dict[from_hex] = name
fh.close()

fh = open("cic.h","w")
fh.write("#ifndef H_CIC\n")
fh.write("#  define H_CIC\n")
fh.write("#  include <stdint.h>\n")
fh.write("const char *get_cic(uint16_t cic);\n")
fh.write("#endif\n")
fh.close()
fh = open("cic.c","w")
fh.write("#include \"cic.h\"\n")
fh.write("const char *get_cic(uint16_t cic) {\n")
fh.write("  switch(cic) {\n")
for i in range(max+1) :
    name = dict.get(i)
    if None != name :
        fh.write("  case 0x%04x: return \"%s\"; break;\n"%(i,name))
fh.write("  }\n  return \"*unregistered*\";\n}\n")
fh.close()
