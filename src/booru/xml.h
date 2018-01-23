#ifndef _XML_H_
#define _XML_H_

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <string>
#include <vector>

namespace AhoViewer
{
    namespace xml
    {
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
        protected:
            Node() = default;
            xmlNodePtr m_xmlNode;
        };

        // The Root node
        class Document : public Node
        {
        public:
            Document(const char *buf, const int size)
            {
                m_xmlDoc = xmlParseMemory(buf, size);

                if (!m_xmlDoc)
                    throw std::runtime_error("Failed to parse XML");
                else
                    m_xmlNode = xmlDocGetRootElement(m_xmlDoc);
            }
            ~Document()
            {
                xmlFreeDoc(m_xmlDoc);
            }

            unsigned long get_n_nodes() const
            {
                return xmlChildElementCount(m_xmlNode);
            }

            const std::vector<Node> get_children() const
            {
                std::vector<Node> children;

                for (xmlNodePtr n = m_xmlNode->children; n; n = n->next)
                {
                    if (n->type == XML_ELEMENT_NODE)
                        children.emplace_back(n);
                }

                return children;
            }
        private:
            xmlDocPtr m_xmlDoc;
        };
    }
}

#endif /* _XML_H_ */
