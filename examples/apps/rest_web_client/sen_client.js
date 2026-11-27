class SenClient {
  constructor() {
    this.eventSource = null;
    this.sseEventTypes = [
      "object_added",
      "object_removed",
      "invoke",
      "event",
      "property",
    ];
  }

  async get(href) {
    try {
      const response = await fetch(href, {
        method: "GET",
        headers: {
          Accept: "application/json",
        },
        credentials: "include",
      });

      if (!response.ok) {
        throw new Error(`HTTP error! Status: ${response.status}`);
      }

      return await response.json();
    } catch (error) {
      console.error("Error GET:", error);
      throw error;
    }
  }

  async post(href) {
    try {
      const response = await fetch(href, {
        method: "POST",
        headers: {
          Accept: "application/json",
        },
        credentials: "include",
      });

      if (!response.ok) {
        throw new Error(`HTTP error! Status: ${response.status}`);
      }

      return await response.json();
    } catch (error) {
      console.error("Error POST:", error);
      throw error;
    }
  }

  async postJSON(href, body) {
    try {
      const response = await fetch(href, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json",
        },
        credentials: "include",
        body: JSON.stringify(body),
      });

      if (!response.ok) {
        throw new Error(`HTTP error! Status: ${response.status}`);
      }

      return await response.json();
    } catch (error) {
      console.error("Error POST JSON:", error);
      throw error;
    }
  }

  async postArgs(href) {
    const $args = document.getElementById("args");
    let args = $args.value.trim();

    args = args.length
      ? args
          .split(",")
          .map((v) => v.trim())
          .map((v) => {
            try {
              return parseInt(v);
            } catch (e) {
              return v;
            }
          })
      : [];

    const data = await this.postJSON(href, args);
    console.log(data);
    return data;
  }

  updateSSE() {
    const $inputSSE = document.getElementById("sse");

    if ($inputSSE.checked) {
      this.enableSSE();
    } else {
      this.disableSSE();
    }
  }

  enableSSE() {
    this.eventSource = new EventSource(`/api/sse`);

    this.sseEventTypes.forEach((name) => {
      this.eventSource.addEventListener(name, (event) => {
        console.log(`[${name}] ${event.data}`);
      });
    });

    console.log("%cSSE enabled", "color: green");
  }

  disableSSE() {
    if (this.eventSource) {
      this.eventSource.close();
      this.eventSource = null;
    }
    console.log("%cSSE disabled", "color: red");
  }

  async signin(clientId) {
    const data = await this.postJSON(`/api/auth`, { id: clientId });
    console.log("Signed in successfully");
    return data;
  }

  async startClientSession() {
    const $clientId = document.getElementById("client-id");
    const clientId = $clientId.value.trim();

    if (!clientId) {
      console.error("Client ID is required");
      return;
    }

    console.log(`Starting session for client: ${clientId}`);
    await this.signin(clientId);
    this.updateSSE();
    await this.getSessions();
  }

  async getSessions() {
    const data = await this.get(`/api/sessions`);
    this.renderSessions(data);
    return data;
  }

  renderSessions(sessions) {
    const $sessions = document.getElementById("sessions");
    $sessions.innerHTML = "";

    sessions.forEach((session) => {
      $sessions.innerHTML += `<div><b>${session}</b></div>`;
    });
  }

  async createInterest() {
    const $name = document.getElementById("interest-name");
    const $query = document.getElementById("interest-query");

    const name = $name.value.trim();
    const query = $query.value.trim();

    const data = await this.postJSON(`/api/interests`, { name, query });
    console.log(data);
    return data;
  }

  async reloadInterests() {
    const interests = await this.get(`/api/interests`);
    this.renderInterests(interests);
    return interests;
  }

  renderInterests(interests) {
    const $interests = document.getElementById("interests");
    $interests.innerHTML = "";

    interests.forEach((interest) => {
      $interests.innerHTML += `<div>
        <span>${JSON.stringify(interest)}</span>
        <button style="max-width:32rem" onclick="senClient.getObjects('${interest.name}');">
          Retrieve Objects
        </button>
      </div>`;
    });
  }

  async getObjects(interestName) {
    const objects = await this.get(`/api/interests/${interestName}/objects`);
    this.renderObjects(objects);
    return objects;
  }

  renderObjects(objects) {
    const $objects = document.getElementById("objects");
    $objects.innerHTML = "";

    objects.forEach((object) => {
      $objects.innerHTML += `<div>
        <b>${object.name}</b> [ ${object.localname} ]
        <button onclick="senClient.getObject('${object.link.href}');">
          Get Object
        </button>
      </div>`;
      console.log(object);
    });
  }

  async getObject(href) {
    const data = await this.get(href);
    console.log(data);
    this.renderObjectDetails(data);
    return data;
  }

  renderObjectDetails(data) {
    const $methods = document.getElementById("methods");
    const $properties = document.getElementById("properties");
    const $events = document.getElementById("events");

    $methods.innerHTML = "";
    $properties.innerHTML = "";
    $events.innerHTML = "";

    data.links.forEach((link) => {
      switch (link.rel) {
        case "method":
          $methods.innerHTML += `<div>
            <b>${link.href}</b>
            <button onclick="senClient.postArgs('${link.href}')">Invoke</button>
          </div>`;
          break;

        case "def":
          $methods.innerHTML += `<div>
            <b>${link.href}</b>
            <button onclick="senClient.get('${link.href}').then(def => console.log(def));">
              Definition
            </button>
          </div>`;
          break;

        case "property":
          $properties.innerHTML += `<div>
            <b>${link.href}</b>
            <button onclick="senClient.get('${link.href}').then(value => console.log(value));">
              Get Value
            </button>
          </div>`;
          break;

        case "property_subscribe":
          $properties.innerHTML += `<div>
            <b>${link.href}</b>
            <button onclick="senClient.post('${link.href}').then(ret => console.log(ret));">
              Subscribe
            </button>
          </div>`;
          break;

        case "property_unsubscribe":
          $properties.innerHTML += `<div>
            <b>${link.href}</b>
            <button onclick="senClient.post('${link.href}').then(ret => console.log(ret));">
              Unsubscribe
            </button>
          </div>`;
          break;
      }
    });
  }

  async init() {
    console.log("Starting SenClient...");
    this.updateSSE();
    await this.getSessions();
  }
}

const senClient = new SenClient();

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", () => senClient.init());
} else {
  senClient.init();
}
