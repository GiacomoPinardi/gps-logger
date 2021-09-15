import xml.etree.ElementTree as et
import sys
from datetime import datetime, timedelta

if len(sys.argv) != 4:
    print('Usage: fix_time_gpx.py <input_file> <output_file> <minutes>')
    sys.exit(1)

input_filename = sys.argv[1]
output_filename = sys.argv[2]
minutes = int(sys.argv[3])

et.register_namespace('', "http://www.topografix.com/GPX/1/0")

tree = et.parse(input_filename)
root = tree.getroot()

for trkpt in root[0][0]:
    for child in trkpt:
        if 'time' in child.tag:
            utc_datetime = datetime.fromisoformat(child.text[:-1])

            new_datetime = utc_datetime + timedelta(minutes=minutes)

            child.text = new_datetime.isoformat() + "Z"

data = et.tostring(root)    
with open(output_filename, 'wb') as f_out:
    f_out.write(data)
