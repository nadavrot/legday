#!/usr/bin/env python
import sys
import os
import csv
import subprocess

def compress_file(input_file, utility_command):
    """
    Compress a file using the specified utility and gzip, returning file sizes.
    """
    # Get original file size
    original_size = os.path.getsize(input_file)
    
    # Determine compression flags based on filename
    flags = ["verify"]
    if 'float32' in input_file.lower():
        flags.append('FP32')
    elif 'bfloat16' in input_file.lower():
        flags.append('BF16')
    elif 'float16' in input_file.lower():
        flags.append('BF16')
    else:
        flags.append('INT8')
    
    # Compress with custom utility
    utility_output = "/tmp/out.bin" + str(id(os)) 
    utility_cmd = [utility_command] + flags + [input_file, utility_output]
    subprocess.run(utility_cmd, check=True)
    utility_size = os.path.getsize(utility_output)
    
    # Compress with gzip
    gzip_output = "/tmp/out.gz" + str(id(os))
    subprocess.run(['gzip', '-c', input_file], stdout=open(gzip_output, 'wb'), check=True)
    gzip_size = os.path.getsize(gzip_output)
    
    return {
        'filename': input_file,
        'original_size': original_size,
        'utility_size': utility_size,
        'gzip_size': gzip_size
    }

def main():
    # Check if utility command is provided
    if len(sys.argv) < 3:
        print("Usage: python script.py <utility_command> <file1> [file2] ...")
        sys.exit(1)
    
    # Extract utility command and input files
    utility_command = sys.argv[1]
    input_files = sys.argv[2:]
    
    # Compress files and collect results
    results = []
    for file in input_files:
        try:
            result = compress_file(file, utility_command)
            results.append(result)
        except Exception as e:
            print(f"Error compressing {file}: {e}")
    
    # Write results to CSV
    if results:
        csv_filename = 'compression_results.csv'
        with open(csv_filename, 'w', newline='') as csvfile:
            fieldnames = ['filename', 'original_size', 'utility_size', 'gzip_size', 
                          'utility_compression_ratio', 'gzip_compression_ratio']
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            
            writer.writeheader()
            for result in results:
                writer.writerow(result)
        
        print(f"Results written to {csv_filename}")
    else:
        print("No files were compressed.")

if __name__ == '__main__':
    main()
