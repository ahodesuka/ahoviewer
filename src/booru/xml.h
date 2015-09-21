#ifndef _XML_H_
#define _XML_H_

#include <libxml/parser.h>
#include <libxml/tree.h>

namespace AhoViewer
{
    class xmlDocument
    {
    public:
        class Node
        {
        public:
            Node(xmlNodePtr n) : m_xmlNode(n) {  }
            ~Node() = default;

            std::string get_attribute(const std::string &name) const
            {
                std::string attr;
                xmlChar *prop = xmlGetProp(m_xmlNode, reinterpret_cast<const xmlChar*>(name.c_str()));

                if (prop)
                {
                    attr = reinterpret_cast<char*>(prop);
                    xmlFree(prop);
                }

                return attr;
            }
        private:
            xmlNodePtr m_xmlNode;
        };

        xmlDocument(const char *buf, const int size)
        {
            m_xmlDoc = xmlParseMemory(buf, size);
        }
        ~xmlDocument()
        {
            xmlFreeDoc(m_xmlDoc);
        }

        unsigned long get_n_nodes() const
        {
            return xmlChildElementCount(xmlDocGetRootElement(m_xmlDoc));
        }

        std::string get_attribute(const std::string &name) const
        {
            return xmlDocument::Node(xmlDocGetRootElement(m_xmlDoc)).get_attribute(name);
        }

        const std::vector<xmlDocument::Node> get_children() const
        {
            std::vector<xmlDocument::Node> children;

            for (xmlNodePtr n = xmlDocGetRootElement(m_xmlDoc)->children; n; n = n->next)
            {
                if (n->type == XML_ELEMENT_NODE)
                    children.push_back(xmlDocument::Node(n));
            }

            return children;
        }
    private:
        xmlDocPtr m_xmlDoc;
    };
}

#endif /* _XML_H_ */
