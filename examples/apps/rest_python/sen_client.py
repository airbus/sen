# === sen_client.py ====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""
This program contains an example of how users can interact with the REST API using Python.

It shows how to sign in, create interests, display details and interact with remote objects.
"""

import json
import threading
import time

import requests
import sseclient


class SenClient:
    """
    Represents the client for the Sen REST component.

    Attributes:
        base_url (str): URL of the Sen REST server.
        session (requests.Session): Active HTTP session.
        sse_event_types (list): List of notifiable event types.
        token (str): Token needed for authentication.
    """

    def __init__(self, base_url="http://localhost"):
        """
        Initialize SenClient.

        Args:
            base_url: base url to use (defaults to localhost)
        """
        self.base_url = base_url.rstrip("/")
        self.session = requests.Session()
        self.sse_event_types = [
            "object_added",
            "object_removed",
            "invoke",
            "event",
            "property",
        ]
        self.token = None

    def get_headers(self, is_sse=False):
        """Gets the headers needed for REST API requests.

        Args:
            is_sse (bool): Whether notifications are enabled.
        """
        header = {
            "Content-Type": "application/json",
        }
        if is_sse:
            header["Accept"] = "text/event-stream"
        else:
            header["Accept"] = "application/json"

        if self.token:
            header["Authorization"] = "Bearer " + self.token
        return header

    def get(self, href):
        """Makes GET request to the REST API.

        Args:
            href (str): Endpoint route.

        Returns:
            dict: Response of the request.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        try:
            url = href if href.startswith("http") else f"{self.base_url}{href}"
            response = self.session.get(url, headers=self.get_headers())
            response.raise_for_status()
            return response.json()

        except requests.RequestException as error:
            print(f"Error GET: {error}")
            raise

    def post(self, href):
        """Makes POST request to the REST API.

        Args:
            href (str): Endpoint route.

        Returns:
            dict: Response of the request.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        try:
            url = href if href.startswith("http") else f"{self.base_url}{href}"
            response = self.session.post(url, headers=self.get_headers())
            response.raise_for_status()
            return response.json()

        except requests.RequestException as error:
            print(f"Error POST: {error}")
            raise

    def post_json(self, href, body):
        """Makes POST request to the REST API with a body attached.

        Args:
            href (str): Endpoint route.
            body (str): Body of the request.

        Returns:
            dict: Response of the request.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        try:
            url = href if href.startswith("http") else f"{self.base_url}{href}"
            response = self.session.post(url, headers=self.get_headers(), json=body)
            response.raise_for_status()
            return response.json()

        except requests.RequestException as error:
            print(f"Error POST JSON: {error}")
            raise

    def post_args(self, href, args_str):
        """Makes POST request to the REST API when some arguments are needed.

        Args:
            href (str): Endpoint route.
            args_str (str): Arguments sent to the endpoint.

        Returns:
            dict: Response of the request.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        args_str = args_str.strip()

        if args_str:
            args = []
            for raw_v in args_str.split(","):
                v = raw_v.strip()
                try:
                    args.append(int(v))
                except ValueError:
                    args.append(v)
        else:
            args = []

        data = self.post_json(href, args)
        return data

    def enable_notifications(self):
        """Enable notifications that are displayed on standard output."""

        def listen_sse():
            """Tries to enable notifications and displays them on the standard output.

            Raises:
                Exception: If there is any error in the request.
            """
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
        """Disable notifications if they are enabled."""
        if self.sse_thread:
            self.sse_thread = None
        print("SSE disabled")

    def signin(self, client_id):
        """Sign in to the REST API to get an API token.

        Args:
            client_id (str): Client id.

        Returns:
            dict: Response containing the API token.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        data = self.post_json("/api/auth", {"id": client_id})
        print("Signed in successfully")
        self.token = data["token"]
        return data

    def start_client_session(self, client_id):
        """Start a client session and get all the sessions if a client id is provided.

        Args:
            client_id (str): Client id.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        if not client_id:
            print("Client ID is required")
            return

        print(f"Starting session for client: {client_id}")
        self.signin(client_id)
        self.get_sessions()

    def get_sessions(self):
        """Get all the sessions.

        Returns:
            list: List containing all the sessions.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        data = self.get("/api/sessions")
        return data

    def print_sessions(self, sessions):
        """Display all the sessions on standard output.

        Args:
            sessions (list): List with all the sessions.
        """
        print("\n=== Sessions ===")
        for session in sessions:
            print(f"   {session}")
        print()

    def create_interest(self, name, query):
        """Create an interest based on a query.

        Args:
            name (str): Name of the interest.
            query (str): Query used to create the interest.

        Returns:
            dict: Dictionary containing the created interest name.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        data = self.post_json("/api/interests", {"name": name, "query": query})
        return data

    def reload_interests(self):
        """Get all the interests created.

        Returns:
            list[dict]: List containing all the interests with their name, bus and session.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        interests = self.get("/api/interests")
        return interests

    def print_interests(self, interests):
        """Display all the interests on standard output.

        Args:
            interests (list[dict]): List containing all the interests.
        """
        print("\n=== Interests ===")
        for interest in interests:
            print(f"   {json.dumps(interest)}")
        print()

    def get_objects(self, interest_name):
        """Get all the objects contained on an interest.

        Args:
            interest_name (str): Name of the interest.

        Returns:
            list[dict]: List containing all the objects with their class name, link, local name,
                        name and object id.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        objects = self.get(f"/api/interests/{interest_name}/objects")
        return objects

    def print_objects(self, objects):
        """Display all the objects contained on an interest on standard output.

        Args:
            objects (list[dict]): List containing all the objects.
        """
        print("\n=== Objects ===")
        for obj in objects:
            print(f"   {obj.get('name')} [ {obj.get('localname')} ]")
            print(f"    Link: {obj.get('link', {}).get('href')}")
        print()

    def get_object(self, interest_name, object_name):
        """
        Get all the object details.

        Args:
            interest_name: Name of the interest
            object_name (str): Name of the object.

        Returns:
            dict: Dictionary with all the object associated with its class name, description, links,
                  local name, name and object id.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        data = self.get(f"/api/interests/{interest_name}/objects/{object_name}")
        return data

    def print_object(self, data):
        """
        Display all the object details.

        Args:
            data (dict): Dictionary containing all the object details.
        """
        methods = []
        properties = []
        events = []

        for link in data.get("links", []):
            rel = link.get("rel")
            href = link.get("href")

            if rel == "method":
                methods.append(href)
            elif rel == "def":
                methods.append(f"{href} (definition)")
            elif rel == "property":
                properties.append(href)
            elif rel == "property_subscribe":
                properties.append(f"{href} (subscribe)")
            elif rel == "property_unsubscribe":
                properties.append(f"{href} (unsubscribe)")

        print("\n=== Object Details ===")
        if methods:
            print("\nMethods:" + "\n".join(f"   {method}" for method in methods))

        if properties:
            print("\nProperties:" + "\n".join(f"   {prop}" for prop in properties))

        if events:
            print("\nEvents:" + "\n".join(f"   {event}" for event in events))
        print()

    def get_method_info(self, interest_name, object_name, method):
        """Get all the method details.

        Args:
            interest_name (str): Name of the interest.
            object_name (str): Name of the object.
            method (str): Name of the method.

        Returns:
            dict: Dictionary containing all the method arguments, description, name and return type of the result.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
        data = self.get(f"/api/interests/{interest_name}/objects/{object_name}/methods/{method}")
        return data

    def print_method_def(self, method_def):
        """Display all the method details.

        Args:
            method_def (dict): Dictionary containing all the method details.
        """
        print(f"Method definition: {method_def}")

    def invoke_method(self, interest_name, object_name, method):
        """Invoke a method.

        Args:
            interest_name (str): Name of the interest.
            object_name (str): Name of the object.
            method (str): Name of the method.

        Returns:
            dict: Dictionary with the method invocation id and its state.

        Raises:
            requests.RequestException: If there is any error in the request.
        """
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
