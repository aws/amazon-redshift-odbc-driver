using System;
using System.Data;
using System.Data.Odbc;
using System.Diagnostics;

namespace RedshiftOdbcTester
{
    class Program
    {
        static int Main(string[] args)
        {
            Console.WriteLine("========================================");
            Console.WriteLine("  Redshift ODBC Connection Tester");
            Console.WriteLine("  Version 1.0 - .NET 4.0");
            Console.WriteLine("========================================");
            Console.WriteLine();

            string dsnName = null;
            string connectionString = null;

            // Parse command line arguments
            if (args.Length > 0)
            {
                if (args[0].ToLower().StartsWith("dsn="))
                {
                    dsnName = args[0].Substring(4);
                    connectionString = "DSN=" + dsnName;
                }
                else if (args[0].ToLower().Contains("driver="))
                {
                    connectionString = args[0];
                }
                else
                {
                    dsnName = args[0];
                    connectionString = "DSN=" + dsnName;
                }
            }
            else
            {
                // Interactive mode
                Console.Write("Enter DSN name (or full connection string): ");
                string input = Console.ReadLine();

                if (string.IsNullOrEmpty(input))
                {
                    Console.WriteLine("Error: No DSN name provided");
                    Console.WriteLine();
                    ShowUsage();
                    return 1;
                }

                if (input.ToLower().Contains("driver=") || input.Contains(";"))
                {
                    connectionString = input;
                }
                else
                {
                    dsnName = input;
                    connectionString = "DSN=" + input;
                }
            }

            Console.WriteLine("Connection String: {0}", connectionString);
            Console.WriteLine();

            return TestConnection(connectionString);
        }

        static int TestConnection(string connectionString)
        {
            OdbcConnection conn = null;
            Stopwatch sw = Stopwatch.StartNew();

            try
            {
                Console.WriteLine("[{0:HH:mm:ss}] Creating connection...", DateTime.Now);
                conn = new OdbcConnection(connectionString);

                Console.WriteLine("[{0:HH:mm:ss}] Opening connection...", DateTime.Now);
                Console.WriteLine("                (Browser may open for authentication)");
                Console.WriteLine();

                conn.Open();

                sw.Stop();

                Console.ForegroundColor = ConsoleColor.Green;
                Console.WriteLine("*** CONNECTION SUCCESSFUL ***");
                Console.ResetColor();
                Console.WriteLine("Time: {0:0.00} seconds", sw.Elapsed.TotalSeconds);
                Console.WriteLine();

                // Get connection info
                Console.WriteLine("Connection Information:");
                Console.WriteLine("  Server: {0}", conn.DataSource);
                Console.WriteLine("  Database: {0}", conn.Database);
                Console.WriteLine("  Driver: {0}", conn.Driver);
                Console.WriteLine("  Server Version: {0}", conn.ServerVersion);
                Console.WriteLine();

                // Run test query
                Console.WriteLine("[{0:HH:mm:ss}] Running test query...", DateTime.Now);

                OdbcCommand cmd = conn.CreateCommand();
                cmd.CommandText = "SELECT CURRENT_USER AS user, CURRENT_DATABASE() AS database, CURRENT_TIMESTAMP AS timestamp, VERSION() AS version";

                OdbcDataReader reader = cmd.ExecuteReader();

                if (reader.Read())
                {
                    Console.WriteLine();
                    Console.WriteLine("Query Results:");
                    Console.WriteLine("  User:      {0}", reader["user"]);
                    Console.WriteLine("  Database:  {0}", reader["database"]);
                    Console.WriteLine("  Timestamp: {0}", reader["timestamp"]);
                    Console.WriteLine("  Version:   {0}", reader["version"].ToString().Substring(0, Math.Min(60, reader["version"].ToString().Length)));
                }

                reader.Close();

                Console.WriteLine();
                Console.ForegroundColor = ConsoleColor.Green;
                Console.WriteLine("*** ALL TESTS PASSED ***");
                Console.ResetColor();
                Console.WriteLine();

                return 0;
            }
            catch (OdbcException ex)
            {
                sw.Stop();

                Console.WriteLine();
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine("*** CONNECTION FAILED ***");
                Console.ResetColor();
                Console.WriteLine();
                Console.WriteLine("ODBC Error:");
                Console.WriteLine("  Message: {0}", ex.Message);
                Console.WriteLine("  Source: {0}", ex.Source);

                if (ex.Errors.Count > 0)
                {
                    Console.WriteLine();
                    Console.WriteLine("Detailed Errors:");
                    for (int i = 0; i < ex.Errors.Count; i++)
                    {
                        OdbcError err = ex.Errors[i];
                        Console.WriteLine("  [{0}] SQLState: {1}", i + 1, err.SQLState);
                        Console.WriteLine("      Message: {0}", err.Message);
                        Console.WriteLine("      Native Error: {0}", err.NativeError);
                    }
                }

                Console.WriteLine();
                Console.WriteLine("Troubleshooting:");
                Console.WriteLine("  1. Check ODBC driver is installed");
                Console.WriteLine("  2. Verify DSN configuration in odbcad32.exe");
                Console.WriteLine("  3. Check connection parameters (server, database, auth)");
                Console.WriteLine("  4. Review logs in: %TEMP%\\Amazon Redshift ODBC Driver\\logs\\");
                Console.WriteLine();

                return 1;
            }
            catch (Exception ex)
            {
                sw.Stop();

                Console.WriteLine();
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine("*** UNEXPECTED ERROR ***");
                Console.ResetColor();
                Console.WriteLine();
                Console.WriteLine("Error: {0}", ex.Message);
                Console.WriteLine("Type: {0}", ex.GetType().Name);
                Console.WriteLine();
                Console.WriteLine("Stack Trace:");
                Console.WriteLine(ex.StackTrace);
                Console.WriteLine();

                return 1;
            }
            finally
            {
                if (conn != null && conn.State == ConnectionState.Open)
                {
                    Console.WriteLine("[{0:HH:mm:ss}] Closing connection...", DateTime.Now);
                    conn.Close();
                }
            }
        }

        static void ShowUsage()
        {
            Console.WriteLine("Usage:");
            Console.WriteLine("  RedshiftOdbcTester.exe <DSN_NAME>");
            Console.WriteLine("  RedshiftOdbcTester.exe DSN=<DSN_NAME>");
            Console.WriteLine("  RedshiftOdbcTester.exe \"<FULL_CONNECTION_STRING>\"");
            Console.WriteLine();
            Console.WriteLine("Examples:");
            Console.WriteLine("  RedshiftOdbcTester.exe AmazonRedshiftOAuth");
            Console.WriteLine("  RedshiftOdbcTester.exe DSN=MyRedshiftDSN");
            Console.WriteLine("  RedshiftOdbcTester.exe \"Driver={Amazon Redshift (x64)};Server=...\"");
            Console.WriteLine();
            Console.WriteLine("Interactive Mode:");
            Console.WriteLine("  Run without arguments to enter DSN name interactively");
            Console.WriteLine();
        }
    }
}
