import xml.etree.ElementTree as et
import sys
import math

def calculate_distance (lat1, lon1, lat2, lon2):
    R = 6371  # Radius of the earth in km
    dLat = deg2rad(lat2-lat1)
    dLon = deg2rad(lon2-lon1)
    a = math.sin(dLat/2) * math.sin(dLat/2) + math.cos(deg2rad(lat1)) * math.cos(deg2rad(lat2)) * math.sin(dLon/2) * math.sin(dLon/2)
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1-a)); 
    d = R * c * 1000; # Distance in meters

    return d

def deg2rad (deg):
    return deg * (math.pi/180)

if len(sys.argv) != 4:
    print('Usage: remove_pause_gpx.py <input_file> <output_file> <threshold_meters>')
    sys.exit(1)

input_filename = sys.argv[1]
output_filename = sys.argv[2]
meters = float(sys.argv[3])

et.register_namespace('', "http://www.topografix.com/GPX/1/0")

tree = et.parse(input_filename)
root = tree.getroot()

to_remove = []
previous_coordinates = None

total_points = 0

for trkpt in root[0][0]:
    total_points += 1
    lat = float(trkpt.attrib['lat'])
    lon = float(trkpt.attrib['lon'])
    
    if previous_coordinates is not None:
        distance = calculate_distance(lat, lon, previous_coordinates[0], previous_coordinates[1])
        if distance < meters:
            # invalid point
            to_remove.append(trkpt)

    previous_coordinates = (lat, lon)

for trkpt in to_remove:
    root[0][0].remove(trkpt)

data = et.tostring(root)    
with open(output_filename, 'wb') as f_out:
    f_out.write(data)

print("Track simplified!")
print("Initial number of points: {}".format(total_points))
print("Points removed: {}".format(len(to_remove)))