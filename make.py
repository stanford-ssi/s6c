
speeds = ['500bps', '5kbps','10kbps']

fields = 'RF_PREAMBLE_TX_LENGTH_9,RF_MODEM_MOD_TYPE_12,RF_MODEM_FREQ_DEV_0_1,RF_MODEM_TX_RAMP_DELAY_8,RF_MODEM_BCR_OSR_1_9,RF_MODEM_AFC_GEAR_7,RF_MODEM_AGC_WINDOW_SIZE_9,RF_MODEM_OOK_CNT1_9,RF_MODEM_RSSI_COMP_1,RF_MODEM_CHFLT_RX1_CHFLT_COE13_7_0_12,RF_MODEM_CHFLT_RX1_CHFLT_COE1_7_0_12,RF_MODEM_CHFLT_RX2_CHFLT_COE7_7_0_12'.split(',')
print("got",len(fields))

out = "const uint8_t speed_config[%d][162] = {\n" % len(speeds)

for i,s in enumerate(speeds):
    with open('/home/joan/virtual/shared/ss6c_%s.h'%s) as f:
        t = f.read().split('\n')
        defs = {}
        for l in t:
            if l.startswith('#define '):
                sp = l.split(' ')
                n = sp[1]
                r = ' '.join(sp[2:])
                defs[n] = r
        config = []
        for x in fields:
            d = defs[x].split(', ')
            #print("%x"%len(d))
            #print(d)
            config.append("0x"+("%02x"%len(d)).upper())
            config.extend(d)
        #1/0
        config.append("0x00")
        print(len(config))
        out += "                {"+','.join(config)+"}"
        if i != len(speeds)-1: out+=",\n"
out+= "};"
print(out)
