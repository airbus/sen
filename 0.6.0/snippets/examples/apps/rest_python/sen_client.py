# === sen_client.py ====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import requests
import json
import sseclient
import time

from typing import List, Dict, Any

class SenClient:
    def __init__(self, base_url = "http://localhost"):
        self.base_url = base_url.rstrip('/')
        self.session = requests.Session()
        self.sse_event_types = [
            "object_added",
            "object_removed",
            "invoke",
            "event",
            "property",
        ]
        self.token = None

    def get_headers(self, is_sse = False):
        header = {"Content-Type": "application/json", }
        if is_sse:
            header["Accept"] = "text/event-stream"
        else:
            header["Accept"] = "application/json"

        if self.token:
            header["Authorization"] = "Bearer " + self.token
        return header

    def get(self, href):
        try:
            url = href if href.startswith('http') else f"{self.base_url}{href}"
            response = self.session.get(
                url,
                headers=self.get_headers()
            )
            response.raise_for_status()
            return response.json()

        except requests.RequestException as error:
            print(f"Error GET: {error}")
            raise

    def post(self, href):
        try:
            url = href if href.startswith('http') else f"{self.base_url}{href}"
            response = self.session.post(
                url,
                headers=self.get_headers()
            )
            response.raise_for_status()
            return response.json()

        except requests.RequestException as error:
            print(f"Error POST: {error}")
            raise

    def post_json(self, href, body):
        try:
            url = href if href.startswith('http') else f"{self.base_url}{href}"
            response = self.session.post(
                url,
                headers= self.get_headers(),
                json=body
            )
            response.raise_for_status()
            return response.json()

        except requests.RequestException as error:
            print(f"Error POST JSON: {error}")
            raise

    def post_args(self, href, args_str):
        args_str = args_str.strip()

        if args_str:
            args = []
            for v in args_str.split(','):
                v = v.strip()
                try:
                    args.append(int(v))
                except ValueError:
                    args.append(v)
        else:
            args = []

        data = self.post_json(href, args)
        return data

    def enable_notifications(self):
        import threading

        def listen_sse():
            try:
                notifications = sseclient.SSEClient(f"{self.base_url}/api/sse", headers=self.get_headers(True))
                for notification in notifications:
                    if notification.event in self.sse_event_types:
                        print(f"Notification received => [{notification.event}] {notification.data}")

            except Exception as e:
                print(f"SSE Error: {e}o")

        self.sse_thread = threading.Thread(target=listen_sse, daemon=True)
        self.sse_thread.start()
        print("Notifications enabled")

    def disable_notifications(self):
        if self.sse_thread:
            self.sse_thread = None
        print("SSE disabled")

    def signin(self, client_id):
        data = self.post_json("/api/auth", {"id": client_id})
        print("Signed in successfully")
        self.token = data["token"]
        return data

    def start_client_session(self, client_id):
        if not client_id:
            print("Client ID is required")
            return

        print(f"Starting session for client: {client_id}")
        self.signin(client_id)
        self.get_sessions()

    def get_sessions(self):
        data = self.get("/api/sessions")
        return data

    def print_sessions(self, sessions):
        print("\n=== Sessions ===")
        for session in sessions:
            print(f"   {session}")
        print()

    def create_interest(self, name, query):
        data = self.post_json("/api/interests", {"name": name, "query": query})
        return data

    def reload_interests(self):
        interests = self.get("/api/interests")
        return interests

    def print_interests(self, interests):
        print("\n=== Interests ===")
        for interest in interests:
            print(f"   {json.dumps(interest)}")
        print()

    def get_objects(self, interest_name):
        objects = self.get(f"/api/interests/{interest_name}/objects")
        return objects

    def print_objects(self, objects):
        print("\n=== Objects ===")
        for obj in objects:
            print(f"   {obj.get('name')} [ {obj.get('localname')} ]")
            print(f"    Link: {obj.get('link', {}).get('href')}")
        print()

    def get_object(self, interest_name, object_name):
        data = self.get(f"/api/interests/{interest_name}/objects/{object_name}")
        return data

    def print_object(self, data):
        methods = []
        properties = []
        events = []

        for link in data.get('links', []):
            rel = link.get('rel')
            href = link.get('href')

            if rel == 'method':
                methods.append(href)
            elif rel == 'def':
                methods.append(f"{href} (definition)")
            elif rel == 'property':
                properties.append(href)
            elif rel == 'property_subscribe':
                properties.append(f"{href} (subscribe)")
            elif rel == 'property_unsubscribe':
                properties.append(f"{href} (unsubscribe)")

        print("\n=== Object Details ===")
        if methods:
            print("\nMethods:")
            for method in methods:
                print(f"   {method}")

        if properties:
            print("\nProperties:")
            for prop in properties:
                print(f"   {prop}")

        if events:
            print("\nEvents:")
            for event in events:
                print(f"   {event}")
        print()

    def get_method_info(self, interest_name, object_name, method):
        data = self.get(f"/api/interests/{interest_name}/objects/{object_name}/methods/{method}")
        return data

    def print_method_def(self, method_def):
        print(f"Method definition: {method_def}")

    def invoke_method(self, interest_name, object_name, method):
        data = self.post_args(f"/api/interests/{interest_name}/objects/{object_name}/methods/{method}/invoke", "")
        return data


if __name__ == "__main__":
    client = SenClient(base_url="http://localhost:8080")
    client.start_client_session("my-client-id")
    client.enable_notifications()
    time.sleep(1)

    sessions = client.get_sessions()
    client.print_sessions(sessions)

    interest = "new_interest"
    query = "SELECT * FROM local.kernel"
    print(f"=== Creating interest '{interest}' with query '{query}' ===\n")
    client.create_interest(interest, query)
    time.sleep(1)
    client.reload_interests()
    client.disable_notifications()

    objects = client.get_objects(interest)
    client.print_objects(objects)

    object = client.get_object(interest, "api")
    client.print_object(object)

    method_def = client.get_method_info(interest, "api", "getConfig")
    client.print_method_def(method_def)

    invoke_ret = client.invoke_method(interest, "api", "getConfig")
    print(invoke_ret)
