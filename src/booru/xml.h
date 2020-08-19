#pragma once

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string>
#include <vector>

namespace AhoViewer::xml
{
    class Node
    {
    public:
        Node(xmlNodePtr n) : m_XmlNode{ n } { }
        ~Node() = default;

        std::string get_attribute(const std::string& name) const
        {
            if (!m_XmlNode)
                return "";

            std::string attr;
            xmlChar* prop{ xmlGetProp(m_XmlNode, reinterpret_cast<const xmlChar*>(name.c_str())) };

            if (prop)
            {
                attr = reinterpret_cast<char*>(prop);
                xmlFree(prop);
            }

            return attr;
        }

        void set_attribute(const std::string& a_name, const std::string& a_value)
        {
            if (!m_XmlNode)
                return;

            const auto *name{ reinterpret_cast<const xmlChar*>(a_name.c_str()) },
                *value{ reinterpret_cast<const xmlChar*>(a_value.c_str()) };

            if (!xmlHasProp(m_XmlNode, name))
                xmlNewProp(m_XmlNode, name, value);
            else
                xmlSetProp(m_XmlNode, name, value);
        }

        std::string get_value() const
        {
            if (!m_XmlNode)
                return "";

            std::string val;
            xmlChar* content{ xmlNodeGetContent(m_XmlNode) };

            if (content)
            {
                val = reinterpret_cast<char*>(content);
                xmlFree(content);
            }

            return val;
        }

        // Returns the value of a child node of this node
        std::string get_value(const std::string& name) const
        {
            if (!m_XmlNode)
                return "";

            xmlNodePtr n{ m_XmlNode->children };
            while (n != nullptr)
            {
                if (xmlStrEqual(n->name, reinterpret_cast<const xmlChar*>(name.c_str())))
                    return Node{ n }.get_value();
                n = n->next;
            }

            return "";
        }

    protected:
        Node() = default;
        xmlNodePtr m_XmlNode{ nullptr };
    };

    // The Root node
    class Document : public Node
    {
    public:
        Document(const char* buf, const size_t size)
        {
            m_XmlDoc = xmlParseMemory(buf, size);

            if (!m_XmlDoc)
                throw std::runtime_error("Failed to parse XML");
            else
                m_XmlNode = xmlDocGetRootElement(m_XmlDoc);
        }
        ~Document() { xmlFreeDoc(m_XmlDoc); }

        unsigned long get_n_nodes() const { return xmlChildElementCount(m_XmlNode); }

        const std::vector<Node> get_children() const
        {
            std::vector<Node> children;

            for (xmlNodePtr n = m_XmlNode->children; n; n = n->next)
            {
                if (n->type == XML_ELEMENT_NODE)
                    children.emplace_back(n);
            }

            return children;
        }

    private:
        xmlDocPtr m_XmlDoc;
    };
}
