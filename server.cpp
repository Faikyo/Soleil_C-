#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cbor.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <tinyxml2.h>
#include <thread>


double sun_x;
double sun_y;

using namespace std;
using namespace tinyxml2;

static void do_drawing(cairo_t *);
static void do_drawing_svg(cairo_t *);

RsvgHandle *svg_handle;
RsvgRectangle viewport;

XMLDocument svg_data;

GtkWidget *window;
GtkWidget *darea;


static void do_drawing_svg(cairo_t * cr, RsvgHandle *handle)
{
    XMLPrinter printer;
    svg_data.Print(&printer);
    svg_handle = rsvg_handle_new_from_data((const unsigned char*) printer.CStr(), printer.CStrSize(), NULL);
    rsvg_handle_render_document (svg_handle, cr, &viewport, NULL);
}
static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{      
    do_drawing_svg(cr, svg_handle);
    return FALSE;
}



void update_svg(XMLElement* element, const char* attribute, double valeur){
    XMLElement * elementParent;
    for (XMLElement* childAttribute = element->FirstChildElement(); childAttribute != NULL; childAttribute = childAttribute->NextSiblingElement()) {
        
        if (strcmp(childAttribute->Value(), "driven") == 0) {
            
            if (strcmp(childAttribute->Attribute("by"), attribute) == 0) {
                const char* target = childAttribute->Attribute("target");
                elementParent = childAttribute->Parent()->ToElement();
                elementParent->SetAttribute(target, valeur);
            }
            return;
        } else {
            update_svg(childAttribute, attribute, valeur);
        }
    }
}

XMLElement* findElementSvg(XMLElement* element, const char* attribute) {
    XMLElement* elementParent;
    for (XMLElement* childAttribute = element->FirstChildElement(); childAttribute != NULL; childAttribute = childAttribute->NextSiblingElement()) {
        if (strcmp(childAttribute->Value(), "circle") == 0) {
            for (XMLElement* drivenElement = childAttribute->FirstChildElement("driven"); drivenElement != NULL; drivenElement = drivenElement->NextSiblingElement("driven")) {
                if (strcmp(drivenElement->Attribute("by"), attribute) == 0) {
                    return drivenElement->Parent()->ToElement();
                }
            }
        } else {
            elementParent = findElementSvg(childAttribute, attribute);
            if (elementParent != NULL) {
                return elementParent;
            }
        }
    }
    return nullptr;
}



void read_initial_sun_position(double &initial_sun_x, double &initial_sun_y) {

    XMLElement *root = svg_data.FirstChildElement("svg");

    XMLElement *soleil_x = findElementSvg(root, "sun_x");
    if (soleil_x) {
        initial_sun_x = atof(soleil_x->Attribute("cx"));
    }

    XMLElement *soleil_y = findElementSvg(root, "sun_y");
    if (soleil_y) {
        initial_sun_y = atof(soleil_y->Attribute("cy"));
    }
}


void update_svg() {

    // Element Contenant la balise svg du fichier 
    XMLElement *svg_sun = svg_data.FirstChildElement("svg");
    
    // Mise à jour des valeurs
    update_svg(svg_sun, "sun_x", sun_x);
    update_svg(svg_sun, "sun_y", sun_y);

}

void update_image() {
    while (true) { 
        update_svg();
        gtk_widget_queue_draw(darea);
        this_thread::sleep_for(chrono::milliseconds(200));
    }
}


void receive_data() {
    cout << "start" << endl;
    const int PORT = 6789;

    // Creation d'une socket UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        cout << "Erreur lors de la création de la socket." << endl;
        exit (1);
    }

    // La structure sockaddr_in definie l'adresse et le port de la socket
    struct sockaddr_in servaddr;

    // Initialiser la structure à 0 avec memset
    memset(&servaddr, 0, sizeof(servaddr));
    
    // Pour IPv4
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(6789);

    // On lie une socket à une adresse et un numéro de port spécifiques.
    bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    cout << "port = " << ntohs(servaddr.sin_port) << endl;

    const int MAX_BUFFER_SIZE = 1024;
    unsigned char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);

    while (true) {
        int n = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&clientaddr, &clientaddrlen);
        if (n < 0) {
            cerr << "Erreur lors de la réception des données." << endl;
        } else {
            cout << "Paquet reçu de " << inet_ntoa(clientaddr.sin_addr) << ":" << ntohs(clientaddr.sin_port) << endl;

            struct cbor_load_result result;
            cbor_item_t *cbor_root = cbor_load(buffer, n, &result);
            
            if (!cbor_isa_map(cbor_root)) {
                cerr << "Expected a CBOR map." << endl;
                cbor_decref(&cbor_root);
            }
            
            cbor_pair *pairs;
            pairs = cbor_map_handle(cbor_root);

            double delta_x = 0.0;
            double delta_y = 0.0;

            for (size_t i = 0; i < cbor_map_size(cbor_root); i++) {
                cbor_item_t *key_item = pairs[i].key;
                cbor_item_t *val_item = pairs[i].value;
                char *key_str = (char*) cbor_string_handle(key_item);
                float val_float = cbor_float_get_float8(val_item);
                if (i == 0) {
                    delta_x = val_float;
                }
                else if (i == 1) {
                    delta_y = val_float;
                }
            }
            cbor_decref(&cbor_root);
            
            sun_x += delta_x;
            sun_y += delta_y;
            cout << "sun_x: " << sun_x << endl << "sun_y: " << sun_y << endl;
        }
    }
}

int main(int argc, char *argv[]) {
    //Initialisation de la bibliothèque GTK et création de la fenêtre et de l'aire de dessin :
    gtk_init(&argc, &argv); 
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    darea = gtk_drawing_area_new();

    //Ajout de l'aire de dessin à la fenêtre et connexion du signal "draw" à la fonction on_draw_event :
    gtk_container_add(GTK_CONTAINER(window), darea);
    g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);

    // On load le fichier maison.svg
    svg_data.LoadFile("maison.svg");

    //Lecture de la position initiale du soleil :
    double initial_sun_x = 0;
    double initial_sun_y = 0;
    read_initial_sun_position(initial_sun_x, initial_sun_y);

    sun_x = initial_sun_x;
    sun_y = initial_sun_y;

    //Connexion du signal "destroy" à la fonction gtk_main_quit pour fermer correctement l'application lors de la fermeture de la fenêtre :
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    //Configuration de la position, de la taille et du titre de la fenêtre :
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window),200, 200); 
    gtk_window_set_title(GTK_WINDOW(window), "GTK window");

    //Affichage de tous les widgets de la fenêtre :
    gtk_widget_show_all(window);

    //Création et démarrage des threads pour recevoir les données et mettre à jour l'image :
    thread receive_data_thread(receive_data);
    thread update_image_thread(update_image);

    //Appel de gtk_main() pour exécuter la boucle principale de l'application GTK,
    //qui gère les événements de l'interface utilisateur et les signaux :
    gtk_main();

    return 0;
}

//le programme entre dans la boucle principale de GTK
//et commence à traiter les événements de l'interface utilisateur et les signaux.
// Les threads receive_data_thread et update_image_thread continuent de fonctionner en arrière-plan,
//en mettant à jour les positions du soleil et en redessinant l'aire de dessin en conséquence.