import csv
import sys

with open(sys.argv[1], newline='') as csvfile:
  reader = csv.DictReader(csvfile,['name','front','back','size','stat','value'])
  running = 0
  unique = {}
  for row in reader:
    if row['stat'] == 'insert_nanos':
      if row['name'] not in unique:
        unique[row['name']] = 0.0
      unique[row['name']] += (float(row['back']) - float(row['front'])) * float(row['value'])
      print(row['name'] + ',' + row['back'] + ',' + str(unique[row['name']]))
