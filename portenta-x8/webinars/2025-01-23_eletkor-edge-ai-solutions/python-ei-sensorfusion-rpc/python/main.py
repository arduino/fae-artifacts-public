#!/usr/bin/env python3
import os
import time
import requests # type: ignore
import signal
import sys
import json
import argparse

from msgpackrpc import Address as RpcAddress, Client as RpcClient, error as RpcError # type: ignore

m4_proxy_host = os.getenv('M4_PROXY_HOST', 'm4proxy')
m4_proxy_port = int(os.getenv('M4_PROXY_PORT', '5001'))
m4_proxy_address = RpcAddress(m4_proxy_host, m4_proxy_port)

sensors = ('temperature', 'humidity', 'pressure', 'gas')

def get_sensors_data_from_m4():
    """Get data from the M4 via RPC (MessagePack-RPC)

    The Arduino sketch on the M4 must implement the methods contained in the `sensors`
    list and returning values from the attached sensor.

    """
    data = any
    try:
        # MsgPack-RPC client to call the M4
        # We need a new client for each call
        get_value = lambda value: RpcClient(m4_proxy_address).call(value)
        data = [get_value(measure) for measure in sensors]

    except RpcError.TimeoutError:
        print("Unable to retrieve sensors data from the M4: RPC Timeout")
    return data

def get_sensors_and_classify(host, port):
    # Construct the URL for the Edge Impulse API for the features upload
    url = f"http://{host}:{port}/api/features"

    while True:
        print("Collecting 400 features from sensors... ", end="")
        
        data = { "features": [] }
        start = time.time()
        for i in range(100):
            sensors = get_sensors_data_from_m4()
            if sensors:
                data["features"].extend(sensors)                            
            time.sleep(100e-6)        
        stop = time.time()
        print(f"Done in {stop - start:.2f} seconds.")
        # print(data)
        
        try:
            response = requests.post(url, json=data)
        except ConnectionError:
            print("Connection Error: retrying later")
            time.sleep(5)
            break

        # Check the response
        if response.status_code != 200:
            print(f"Failed to submit the features. Status Code: {response.status_code}")
            break
            
        print("Successfully submitted features. ", end="")

        # Process the JSON response to extract the bounding box with the maximum value
        data = response.json()
        classification = data['result']['classification']
        print(f'Classification: {classification}')

        # Find the class with the maximum value
        if classification:
            label = max(classification, key=classification.get)
            value = classification[label]

            print(f"{label}: {value}")

            # Create a JSON string with the label and value
            request_data = json.dumps({'label': label, 'value': value})

            res = 0

            try:
                client = RpcClient(m4_proxy_address)
                res = client.call('classification', request_data)
            except RpcError.TimeoutError:
                print("Unable to retrieve data from the M4: RPC Timeout.")

            print(f"Sent to {m4_proxy_host} on port {m4_proxy_port}: {request_data}. Result is {res}.")
        else:
            print("No classification found.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Get 1 second of sensors data and send to inference container for classification")
    parser.add_argument("host", help="The hostname or IP address of the inference server")
    parser.add_argument("port", type=int, help="The port number of the inference server")

    # Parse the arguments
    args = parser.parse_args()

    # Signal handler to handle Ctrl+C (SIGINT) gracefully
    def signal_handler(_sig, _frame):
        print("Exiting...")
        sys.exit(0)

    # Register signal handler for graceful exit on Ctrl+C
    signal.signal(signal.SIGINT, signal_handler)

    print("Classifying the Room with Sensor Fusion and AI... Press Ctrl+C to stop.")

    # Run the capture, upload, and process function
    get_sensors_and_classify(args.host, args.port)

