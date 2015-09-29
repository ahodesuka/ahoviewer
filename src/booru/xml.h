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

            void set_attribute(const std::string &_name, const std::string &_value)
            {
                const xmlChar *name  = reinterpret_cast<const xmlChar*>(_name.c_str()),
                              *value = reinterpret_cast<const xmlChar*>(_value.c_str());

                if (!xmlHasProp(m_xmlNode, name))
                    xmlNewProp(m_xmlNode, name, value);
                else
                    xmlSetProp(m_xmlNode, name, value);
            }

            std::string get_value() const
            {
                std::string val;
                xmlChar *content = xmlNodeGetContent(m_xmlNode);

                if (content)
                {
                    val = reinterpret_cast<char*>(content);
                    xmlFree(content);
                }

                return val;
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

        void set_attribute(const std::string &name, const std::string &value)
        {
            xmlDocument::Node(xmlDocGetRootElement(m_xmlDoc)).set_attribute(name, value);
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
