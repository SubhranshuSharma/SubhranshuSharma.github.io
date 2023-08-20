function redirectFunction() {
    var pdfLink = "./certs/certificate_of_earnings.pdf";
    var fallbackLink = "https://github.com/SubhranshuSharma/SubhranshuSharma.github.io/blob/master/certs/certificate_of_earnings.pdf";
    try {
        if(navigator.userAgent.toLowerCase().indexOf("android") > -1) {
            window.open(fallbackLink, "_blank");
        } else {
            window.open(pdfLink, "_blank");
        }
    } catch (error) {
        window.open(pdfLink, "_blank");
    }
}