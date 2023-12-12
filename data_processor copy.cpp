#include <iostream>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <unistd.h>
#include "json.hpp" 
#include "mqtt/client.h" 
#include <random>
#include <mutex>
#include <sys/socket.h>
#include <vector>

#define QOS 1
#define BROKER_ADDRESS "tcp://localhost:1883"
#define GRAPHITE_HOST /*"127.0.0.1"*/"graphite"
#define GRAPHITE_PORT 2003

std::random_device rd;
std::mt19937 mt(rd());
std::uniform_int_distribution<int> sensor_simulate(0, 100);

std::mutex mtx;

    // Variáveis que precisamos alterar futuramente

    struct Sensors
    {
        std::string sensor_id;
        std::string data_type;
        int data_interval;
        int timestamp[2];
        int value[2];
        //colocou uma thread aqui
    };

    std::vector<Sensors> sensors;
    Sensors cpu = Sensors::Sensors("CPU_Percentage_Used", "percent", 30, {0, 0}, {0, 0});
    sensors.push_back(cpu);
    sensors.push_back(Sensors("Memory_Percentage_Used", "percent", 25, {0, 0}, {0, 0}));

    struct Machines
    {
        std::string machine_id;
        std::vector<Sensors> sensors_machine;
    };


int string_to_miliseconds(const std::string& timestamp_str)
{
    int milisseconds = 0;

    /**
      *Montar a função
      *Não sei com vem a string
    */

    return milisseconds;
}


void post_metric(const std::string& machine_id, const std::string& sensor_id, const std::string& timestamp_str, const int value) {
    // Create a Graphite message.
   // graphite::Message message(machine_id + "." + sensor_id, value, timestamp_seconds);

    // Send the Graphite message.
    //graphite::Connection connection(GRAPHITE_HOST, GRAPHITE_PORT);
    //connection.send(machine_id + "." + sensor_id, value, timestamp_str);

    int novo_socket = socket(AF_INET, SOCK_STREAM, 0); //criando uma comunicação sincrona

    // Retorna erro quando nao criar o socket
    if (novo_socket == -1)
    {
        std::cerr << "Error: Failed to create socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr address;//struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(GRAPHITE_PORT);
    //inet_aton(AF_INET, GRAPHITE_HOST, &graphite_address.sin_addr);
    address.sin_addr.s_addr = inet_addr(GRAPHITE_HOST);

    int bind_result = bind(novo_socket, (struct sockaddr *)&address, sizeof(address));
    if (bind_result == -1)
    {
        // Erro ao associar o socket
        std::cerr << "Error: Failed to bind socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }

    int listen_result = listen(novo_socket, 5);
    if (listen_result == -1)
    {
        // Erro ao colocar o socket no modo de escuta
        std::cerr << "Error: Failed to listen socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }

    int connect_result = connect(novo_socket, (struct sockaddr *)&address, sizeof(address));
    if (connect_result == -1)
    {
        // Erro ao conectar o socket
        std::cerr << "Error: Failed to connect socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }

    int accept_result = accept(novo_socket, (struct sockaddr *)&address, &address_len);
    if (accept_result == -1) 
    {
        // Erro ao aceitar a conexão
        std::cerr << "Error: Failed to accept socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }

    std::string post_metric = machine_id + "." + sensor_id + "|" + timestamp_str;

    ssize_t send_result = send(novo_socket,
                               post_metric.c_str(), // Ponteiro para a string de dados métricos.
                               post_metric.size(), // Tamanho da string de dados métricos.
                               0);
    if (send_result == -1) 
    {
        // Erro ao aceitar a conexão
        std::cerr << "Error: Failed to send data" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }

    close(novo_socket);
    return 0;
}

void send_inactivity_alarms(/*const std::chrono::seconds& inactivity_timeout*/) {
    // Armazene o timestamp da última leitura recebida para cada sensor.
    /*    std::map<std::string, std::chrono::time_point<std::chrono::system_clock>> last_reading_timestamps;

        
        // Verifique se o tempo limite de inatividade foi ultrapassado para qualquer sensor.
        for (const auto& sensor : sensors) {
            // Obtenha o timestamp da última leitura recebida.
            auto last_reading_timestamp = last_reading_timestamps[sensor.sensor_id];

            // Verifique se o tempo limite de inatividade foi ultrapassado.
            if (std::chrono::system_clock::now() - last_reading_timestamp > inactivity_timeout) {
                // Envie um alarme para o Graphite.
                post_metric(machine_id, "alarms.inactive", sensor.sensor_id, 1);
            }
        }
    */
    while(1)
    {
        for (const auto& sensor : sensors) {        
            mtx.lock();

            int last_read = string_to_miliseconds(sensor.timestamp[1]);

            // Verifique se o tempo limite de inatividade foi ultrapassado.
            if (std::chrono::system_clock::now() - last_read > sensor.data_interval*10) {
                // Envie um alarme para o Graphite.
                post_metric(machine_id, sensor.sensor_id, "alarms.inactive", 1);
            }

            mtx.unlock();
        }
        
        std::this_thread::sleep_for(std::chrono::miliseconds(20));
    }
        
}

std::vector<std::string> split(const std::string &str, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

void connect_to_mqtt_broker(mqtt::async_client& client, const std::string& broker_address, const std::string& client_id) {
    // Crie uma nova conexão MQTT.
    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    // Tente conectar ao broker.
    try {
        client.connect(connOpts);
    } catch (mqtt::exception& e) {
        // Erro de conexão.
        std::cerr << "Error: Failed to connect" << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Assine o tópico /sensors/#.
    client.subscribe("/sensors/#", QOS);
}

void data_slew_rate(const std::string& machine_id, const std::string& sensor_id, 
                    const std::string& timestamp_old, const int value_old, 
                    const std::string& timestamp_new, const int value_new) {

    
    int interval = string_to_miliseconds(timestamp_new) - string_to_miliseconds(timestamp_old);
    int slew_rate = (value_new - value_old) / interval;

    mtx.lock();
    post_metric(machine_id, sensor.sensor_id, "Slew rate data: ", slew_rate);
    mtx.unlock();

}

int CPU_sensor(/*const std::chrono::seconds& timestamp, const int& value*/)
{
    while(1)
    {
        mtx.lock();

        sensors[0].timestamp[0] = sensors[0].timestamp[1];
        sensors[0].timestamp[1] = std::chrono::system_clock::now();

        sensors[0].value[0] = sensors[0].value[1];
        sensors[0].value[1] = sensor_simulate(mt);

        mtx.unlock();
        std::this_thread::sleep_for(std::chrono::miliseconds(sensors[0].data_interval));
    }
    
}

void memory_sensor(/*const std::chrono::seconds& timestamp, const int& value*/)
{
    while(1)
    {    
        mtx.lock();

        sensors[1].timestamp[0] = sensors[1].timestamp[1];
        sensors[1].timestamp[1] = std::chrono::system_clock::now();

        sensors[1].value[0] = sensors[1].value[1];
        sensors[1].value[1] = sensor_simulate(mt);

        mtx.unlock();
        std::this_thread::sleep_for(std::chrono::miliseconds(sensors[1].data_interval));
    }
}

int main(int argc, char* argv[]) {
    std::string clientId = "clientId";
    mqtt::async_client client(BROKER_ADDRESS, clientId);


    // Create an MQTT callback.
    class callback : public virtual mqtt::callback {
    public:

        void message_arrived(mqtt::const_message_ptr msg) override {
            auto j = nlohmann::json::parse(msg->get_payload());

            std::string topic = msg->get_topic();
            auto topic_parts = split(topic, '/');
            /*std::string machine_id = topic_parts[2];
            std::string sensor_id = topic_parts[3];*/

            std::string sensor_id = topic_parts[4];

            std::string timestamp = j["timestamp"];
            int value = j["value"];
            post_metric(machine_id, sensor_id, timestamp, value);
            //É AQUI Q PEGA OS DADOS?
            //sensors.push_back({"CPU_Percentage_Used", "", 30, {0, 0}, {0, 0}});
            //sensors.push_back({"Memory_Percentage_Used", "", 25, {0, 0}, {0, 0}});

            if(sensor_id == "001") sensors[0] = {"CPU_Percentage_Used", "Percentil", 30, {0, timestamp}, {0, value}};
            else sensors[1] = {"Memory_Percentage_Used", "Percentil", 30, {0, timestamp}, {0, value}};
        }
    };

    callback cb;
    client.set_callback(cb);

    // Connect to the MQTT broker.
    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    try {
        client.connect(connOpts);
        client.subscribe("/sensors/#", QOS);
    } catch (mqtt::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Iniciando processamentos em paralelo
        // Simulação dos sensores
        //std::thread CPU_sensor_simulate(CPU_sensor);
        //std::thread Memory_sensor_simulate(memory_sensor);

        // Processamento dos dados
        std::thread inactive_alarm_process(send_inactivity_alarms);
        std::thread slew_rate_process(data_slew_rate);

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        //graphite::Message message(machine_id + "." + sensor_id, value, timestamp_seconds);

        /**
         * o que tem que fazer:
         
         inscrever no tópico /sensor_monitor. Como cria esse tópico?


         manda msg
         persistência: metric-patha ser usado deve ser machine-id.sendor-id Que poha é essa???

         espera o alarme de inatividade send_inactivity_alarms()
         persistencia alarme: metric path do tipo machine-id.alarms.alarm-type

         segundo tipo de processamento. Analise de custo: do_custom_processing()

         */
    }


    // ID DA MAQUINA
    std::string uuid_string = get_machine_uuid();   

    return EXIT_SUCCESS;
}