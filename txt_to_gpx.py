import xml.etree.ElementTree as et
import sys

if len(sys.argv) != 3:
    print('Usage: txt_to_gpx.py <input_file> <output_file>')
    sys.exit(1)

input_filename = sys.argv[1]
output_filename = sys.argv[2]

gpx = et.Element('gpx')
gpx.set('version', '1.0')
gpx.set('xmlns:xsi', 'http://www.w3.org/2001/XMLSchema-instance')
gpx.set('xmlns', 'http://www.topografix.com/GPX/1/0')
gpx.set('xsi:schemaLocation', 'http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd')

with open(input_filename, 'r') as f_in:
    trk = et.SubElement(gpx, 'trk')
    trkseg = et.SubElement(trk, 'trkseg')
    for line in f_in:
        fields = line.split(',')
        if len(fields) == 5:            
            trkpt = et.SubElement(trkseg, 'trkpt')
            trkpt.set('lat', fields[1])
            trkpt.set('lon', fields[2])

            time = et.SubElement(trkpt, 'time')
            time.text = '20'+fields[0]
            ele = et.SubElement(trkpt, 'ele')
            ele.text = fields[3]
            speed = et.SubElement(trkpt, 'speed')
            speed.text = fields[4]

    data = et.tostring(gpx)
    
    with open(output_filename, 'wb') as f_out:
        f_out.write(str.encode('<?xml version="1.0" encoding="UTF-8" ?>'))
        f_out.write(data)

