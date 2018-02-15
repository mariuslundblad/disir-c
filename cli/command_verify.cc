#include <iostream>
#include <algorithm>
#include <memory>
#include <set>

#include <disir/disir.h>
#include <disir/fslib/util.h>
#include <disir/fslib/json.h>

#include <disir/cli/command_verify.h>
#include <disir/cli/args.hxx>

using namespace disir;

CommandVerify::CommandVerify(void)
    : Command ("verify")
{
}

int
CommandVerify::handle_command (std::vector<std::string> &args)
{
    std::stringstream group_description;
    args::ArgumentParser parser ("Verify configuration entries and their associated molds.");

    setup_parser (parser);
    parser.Prog ("disir verify");

    args::HelpFlag help (parser, "help", "Display the list help menu and exit.",
                         args::Matcher{'h', "help"});

    group_description << "Specify the group to operate on. The loaded default is: "
                      << m_cli->group_id();
    args::ValueFlag<std::string> opt_group_id (parser, "NAME", group_description.str(),
                                               args::Matcher{"group"});

    args::Flag opt_mold (parser, "mold",
                         "Verify all molds instead of configs.",
                         args::Matcher{"mold"});

    args::ValueFlag<std::string> opt_text_mold (parser, "TEXT MOLD",
                                                "Verify mold from disk.",
                                                args::Matcher{"text-mold"});
    args::PositionalList<std::string> opt_entries (parser, "entry",
                                                   "A list of entries to verify.");

    try
    {
        parser.ParseArgs (args);
    }
    catch (args::Help)
    {
        std::cout << parser;
        return (0);
    }
    catch (args::ParseError e)
    {
        std::cerr << "ParseError: " << e.what() << std::endl;
        std::cerr << "See '" << m_cli->m_program_name << " --help'" << std::endl;
        return (1);
    }
    catch (args::ValidationError e)
    {
        std::cerr << "ValidationError: " << e.what() << std::endl;
        std::cerr << "See '" << m_cli->m_program_name << " --help'" << std::endl;
        return (1);
    }


    if (opt_text_mold)
    {
        enum disir_status status;
        struct disir_mold *mold = NULL;
        struct stat statbuf;
        std::string filepath_mold;
        char filepath[4096];
        char oe_filepath[4096];
        char *oe_ref = oe_filepath;

        filepath_mold = args::get (opt_text_mold);

        std::cout << "mold text: " << filepath_mold << std::endl;
        status = fslib_mold_resolve_entry_filepath (m_cli->disir(), filepath_mold.c_str(), filepath,
                                                    oe_filepath, &statbuf, NULL);
        if (status != DISIR_STATUS_OK)
        {
            std::cout << "  " << disir_error (m_cli->disir()) << std::endl;
            return (1);
        }

        if (oe_filepath[0] == '\0')
        {
            oe_ref = NULL;
        }

        // TODO: Check file extension - switch on available molds
        // XXX Hardcode to json unserialize for now
        status = dio_json_unserialize_mold_filepath (m_cli->disir(), filepath, oe_ref, &mold);
        print_verify (status, filepath_mold.c_str(), NULL, mold);

        // TODO: Take entries arguments and verify them as config with this mold ( if the mold is OK)

        // Cleanup
        if (mold)
        {
            disir_mold_finished (&mold);
        }

        return (0);
    }

    if (opt_group_id && setup_group (args::get(opt_group_id)))
    {
        return (1);
    }

    // Get the set of entries to verify
    std::set<std::string> entries_to_verify;
    if (opt_entries)
    {
        m_cli->verbose() << "Verifying entries in user supplied list." << std::endl;
        for (const auto& entry : args::get (opt_entries))
        {
            entries_to_verify.insert (entry);
        }
    }
    else
    {
        m_cli->verbose() << "Verifying all available entries." << std::endl;

        enum disir_status status;
        struct disir_entry *entries;
        struct disir_entry *next;
        struct disir_entry *current;

        if (opt_mold)
        {
            status = disir_mold_entries (m_cli->disir(), m_cli->group_id().c_str(), &entries);
        }
        else
        {
            status = disir_config_entries (m_cli->disir(), m_cli->group_id().c_str(), &entries);
        }
        if (status != DISIR_STATUS_OK)
        {
            std::cerr << "Failed to retrieve available entries: "
                      << disir_error (m_cli->disir()) << std::endl;
            return (-1);
        }

        current = entries;
        while (current != NULL)
        {
            next = current->next;

            entries_to_verify.insert (std::string(current->de_entry_name));

            disir_entry_finished (&current);
            current = next;
        }
    }

    // Iterate each entry, attempt to verify and print result
    std::cout << "In group " << m_cli->group_id() << std::endl;
    if (entries_to_verify.empty())
    {
        std::cout << "  There are no available entries." << std::endl;
        return (0);
    }

    m_cli->verbose() << "There are " << entries_to_verify.size()
                     << " entries to verify." << std::endl;
    std::cout << std::endl;
    for (const auto& entry : entries_to_verify)
    {
        // We simply read the config entry - the return status shall indicate whether it is invalid or not
        enum disir_status status;
        struct disir_config *config = NULL;
        struct disir_mold *mold = NULL;

        if (opt_mold)
        {
            status = disir_mold_read (m_cli->disir(), m_cli->group_id().c_str(),
                                      entry.c_str(), &mold);
        }
        else
        {
            status = disir_config_read (m_cli->disir(), m_cli->group_id().c_str(),
                                        entry.c_str(), NULL, &config);
        }

        print_verify (status, entry.c_str(), config, mold);
        if (config)
            disir_config_finished (&config);
        if (mold)
            disir_mold_finished (&mold);
    }
    std::cout << std::endl;

    return (0);
}

